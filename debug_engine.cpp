#include "debug_engine.h"

#include <DbgHelp.h>
#include <TlHelp32.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

#pragma comment(lib, "dbghelp.lib")

using JsonDocument = nlohmann::json;

struct CodeViewHeaderPdb70
{
    DWORD signatureIdentifier;
    GUID guidSignature;
    DWORD ageCount;
    char pdbPathBuffer[1];
};

struct SymbolEnumerationContext
{
    HANDLE targetProcessHandle;
    STACKFRAME64 currentStackFrame;
    std::vector<VariableInformation> *variableCollection;
};

static std::string formatDateTimeString(const std::chrono::system_clock::time_point &timePoint)
{
    std::time_t systemTime = std::chrono::system_clock::to_time_t(timePoint);
    std::tm localTimeStructure = {0};
    localtime_s(&localTimeStructure, &systemTime);
    std::stringstream stringStream;
    stringStream << std::put_time(&localTimeStructure, "%Y%m%d_%H%M%S");
    return stringStream.str();
}

static std::string formatHexValueString(DWORD64 numericValue)
{
    std::stringstream stringStream;
    stringStream << "0x" << std::hex << std::uppercase << numericValue;
    return stringStream.str();
}

static std::string describeExceptionCode(DWORD exceptionCode)
{
    switch (exceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return "Access Violation (0xC0000005)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "Array Bounds Exceeded (0xC000008C)";
    case EXCEPTION_BREAKPOINT:
        return "Breakpoint Triggered (0x80000003)";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "Datatype Misalignment (0x80000002)";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "Floating Point Denormal Operand (0xC000008D)";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "Floating Point Division By Zero (0xC000008E)";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "Floating Point Inexact Result (0xC000008F)";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "Floating Point Invalid Operation (0xC0000090)";
    case EXCEPTION_FLT_OVERFLOW:
        return "Floating Point Overflow (0xC0000091)";
    case EXCEPTION_FLT_STACK_CHECK:
        return "Floating Point Stack Check (0xC0000092)";
    case EXCEPTION_FLT_UNDERFLOW:
        return "Floating Point Underflow (0xC0000093)";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "Illegal Instruction (0xC000001D)";
    case EXCEPTION_IN_PAGE_ERROR:
        return "In-Page Error (0xC0000006)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "Integer Division By Zero (0xC0000094)";
    case EXCEPTION_INT_OVERFLOW:
        return "Integer Overflow (0xC0000095)";
    case EXCEPTION_INVALID_DISPOSITION:
        return "Invalid Disposition (0xC0000026)";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "Noncontinuable Exception (0xC0000025)";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "Privileged Instruction (0xC0000096)";
    case EXCEPTION_SINGLE_STEP:
        return "Single Step Trap (0x80000004)";
    case EXCEPTION_STACK_OVERFLOW:
        return "Stack Overflow (0xC00000FD)";
    case 0xC0000374:
        return "Heap Corruption (0xC0000374)";
    default:
        return "Unknown Exception (" + formatHexValueString(exceptionCode) + ")";
    }
}

static BOOL CALLBACK EnumerateSymbolsCallback(PSYMBOL_INFO symbolInformation, ULONG symbolSize, PVOID userContextPointer)
{
    if (!symbolInformation || !userContextPointer)
    {
        return TRUE;
    }

    SymbolEnumerationContext *context = reinterpret_cast<SymbolEnumerationContext *>(userContextPointer);
    VariableInformation variableInfo;
    variableInfo.variableName = symbolInformation->Name;
    variableInfo.size = symbolInformation->Size;
    variableInfo.address = symbolInformation->Address;

    if (symbolInformation->Flags & SYMFLAG_PARAMETER)
    {
        variableInfo.typeName = "Parameter";
    }
    else if (symbolInformation->Flags & SYMFLAG_LOCAL)
    {
        variableInfo.typeName = "Local Variable";
    }
    else
    {
        variableInfo.typeName = "Variable";
    }

    if (symbolInformation->Flags & SYMFLAG_REGREL)
    {
        DWORD64 memoryAddress = context->currentStackFrame.AddrFrame.Offset + symbolInformation->Address;
        variableInfo.address = memoryAddress;
        DWORD64 memoryValue = 0;
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(context->targetProcessHandle, reinterpret_cast<LPCVOID>(memoryAddress), &memoryValue, sizeof(DWORD64), &bytesRead) && bytesRead > 0)
        {
            variableInfo.valueRepresentation = formatHexValueString(memoryValue);
        }
        else
        {
            variableInfo.valueRepresentation = "unreadable memory";
        }
    }
    else if (symbolInformation->Address != 0)
    {
        DWORD64 memoryValue = 0;
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(context->targetProcessHandle, reinterpret_cast<LPCVOID>(symbolInformation->Address), &memoryValue, sizeof(DWORD64), &bytesRead) && bytesRead > 0)
        {
            variableInfo.valueRepresentation = formatHexValueString(memoryValue);
        }
        else
        {
            variableInfo.valueRepresentation = "memory unavailable";
        }
    }

    context->variableCollection->push_back(variableInfo);
    return TRUE;
}

DebugInformationValidationResult validateDynamicLinkLibraryDebugInfo(const std::string &dynamicLinkLibraryPath)
{
    DebugInformationValidationResult validationResult;

    if (!std::filesystem::exists(dynamicLinkLibraryPath))
    {
        validationResult.errorMessage = "Dynamic link library file does not exist at path: " + dynamicLinkLibraryPath;
        return validationResult;
    }

    std::ifstream binaryStream(dynamicLinkLibraryPath, std::ios::binary);
    if (!binaryStream.is_open())
    {
        validationResult.errorMessage = "Failed to open dynamic link library file for reading";
        return validationResult;
    }

    IMAGE_DOS_HEADER dosHeader = {0};
    binaryStream.read(reinterpret_cast<char *>(&dosHeader), sizeof(IMAGE_DOS_HEADER));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        validationResult.errorMessage = "File is not a valid PE binary (invalid DOS signature)";
        return validationResult;
    }

    binaryStream.seekg(dosHeader.e_lfanew, std::ios::beg);
    DWORD ntSignature = 0;
    binaryStream.read(reinterpret_cast<char *>(&ntSignature), sizeof(DWORD));
    if (ntSignature != IMAGE_NT_SIGNATURE)
    {
        validationResult.errorMessage = "File is not a valid PE binary (invalid NT signature)";
        return validationResult;
    }

    IMAGE_FILE_HEADER fileHeader = {0};
    binaryStream.read(reinterpret_cast<char *>(&fileHeader), sizeof(IMAGE_FILE_HEADER));

    DWORD debugDirectoryVirtualAddress = 0;
    DWORD debugDirectoryDataSize = 0;

    if (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        IMAGE_OPTIONAL_HEADER64 optionalHeader64 = {0};
        binaryStream.read(reinterpret_cast<char *>(&optionalHeader64), sizeof(IMAGE_OPTIONAL_HEADER64));
        if (optionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG)
        {
            debugDirectoryVirtualAddress = optionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
            debugDirectoryDataSize = optionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
        }
    }
    else if (fileHeader.Machine == IMAGE_FILE_MACHINE_I386)
    {
        IMAGE_OPTIONAL_HEADER32 optionalHeader32 = {0};
        binaryStream.read(reinterpret_cast<char *>(&optionalHeader32), sizeof(IMAGE_OPTIONAL_HEADER32));
        if (optionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG)
        {
            debugDirectoryVirtualAddress = optionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
            debugDirectoryDataSize = optionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
        }
    }
    else
    {
        validationResult.errorMessage = "Unsupported machine architecture in PE header";
        return validationResult;
    }

    if (debugDirectoryVirtualAddress == 0 || debugDirectoryDataSize == 0)
    {
        validationResult.errorMessage = "Dynamic link library was not compiled with debug information (missing debug directory entry)";
        return validationResult;
    }

    std::vector<IMAGE_SECTION_HEADER> sectionHeaders(fileHeader.NumberOfSections);
    binaryStream.read(reinterpret_cast<char *>(sectionHeaders.data()), fileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));

    DWORD debugDirectoryFileOffset = 0;
    for (const auto &singleSection : sectionHeaders)
    {
        DWORD sectionVirtualAddress = singleSection.VirtualAddress;
        DWORD sectionVirtualSize = singleSection.Misc.VirtualSize;
        if (debugDirectoryVirtualAddress >= sectionVirtualAddress &&
            debugDirectoryVirtualAddress < sectionVirtualAddress + sectionVirtualSize)
        {
            debugDirectoryFileOffset = singleSection.PointerToRawData + (debugDirectoryVirtualAddress - sectionVirtualAddress);
            break;
        }
    }

    if (debugDirectoryFileOffset == 0)
    {
        validationResult.errorMessage = "Failed to resolve file offset of debug directory";
        return validationResult;
    }

    binaryStream.seekg(debugDirectoryFileOffset, std::ios::beg);
    size_t debugEntryCount = debugDirectoryDataSize / sizeof(IMAGE_DEBUG_DIRECTORY);
    std::vector<IMAGE_DEBUG_DIRECTORY> debugEntries(debugEntryCount);
    binaryStream.read(reinterpret_cast<char *>(debugEntries.data()), debugDirectoryDataSize);

    bool codeViewEntryFound = false;
    IMAGE_DEBUG_DIRECTORY codeViewDirectory = {0};

    for (const auto &singleEntry : debugEntries)
    {
        if (singleEntry.Type == IMAGE_DEBUG_TYPE_CODEVIEW && singleEntry.SizeOfData > 0)
        {
            codeViewDirectory = singleEntry;
            codeViewEntryFound = true;
            break;
        }
    }

    if (!codeViewEntryFound)
    {
        validationResult.errorMessage = "Dynamic link library lacks CodeView debug directory entry";
        return validationResult;
    }

    binaryStream.seekg(codeViewDirectory.PointerToRawData, std::ios::beg);
    DWORD codeViewSignature = 0;
    binaryStream.read(reinterpret_cast<char *>(&codeViewSignature), sizeof(DWORD));

    std::string embeddedPdbPath = "";

    if (codeViewSignature == 0x53445352)
    {
        binaryStream.seekg(codeViewDirectory.PointerToRawData + 24, std::ios::beg);
        size_t pathBufferLength = codeViewDirectory.SizeOfData > 24 ? codeViewDirectory.SizeOfData - 24 : 0;
        if (pathBufferLength > 0)
        {
            std::vector<char> pathBuffer(pathBufferLength, 0);
            binaryStream.read(pathBuffer.data(), pathBufferLength);
            embeddedPdbPath = std::string(pathBuffer.data());
        }
    }
    else if (codeViewSignature == 0x3031424E)
    {
        binaryStream.seekg(codeViewDirectory.PointerToRawData + 16, std::ios::beg);
        size_t pathBufferLength = codeViewDirectory.SizeOfData > 16 ? codeViewDirectory.SizeOfData - 16 : 0;
        if (pathBufferLength > 0)
        {
            std::vector<char> pathBuffer(pathBufferLength, 0);
            binaryStream.read(pathBuffer.data(), pathBufferLength);
            embeddedPdbPath = std::string(pathBuffer.data());
        }
    }
    else
    {
        validationResult.errorMessage = "Unrecognized CodeView debug signature in binary";
        return validationResult;
    }

    std::filesystem::path dllFilesystemPath(dynamicLinkLibraryPath);
    std::filesystem::path resolvedPdbPath = "";

    if (!embeddedPdbPath.empty() && std::filesystem::exists(embeddedPdbPath))
    {
        resolvedPdbPath = embeddedPdbPath;
    }
    else if (!embeddedPdbPath.empty())
    {
        std::filesystem::path candidateSibling = dllFilesystemPath.parent_path() / std::filesystem::path(embeddedPdbPath).filename();
        if (std::filesystem::exists(candidateSibling))
        {
            resolvedPdbPath = candidateSibling;
        }
    }

    if (resolvedPdbPath.empty())
    {
        std::filesystem::path extensionReplaced = dllFilesystemPath;
        extensionReplaced.replace_extension(".pdb");
        if (std::filesystem::exists(extensionReplaced))
        {
            resolvedPdbPath = extensionReplaced;
        }
    }

    if (resolvedPdbPath.empty())
    {
        validationResult.errorMessage = "Matching PDB symbol file was not found for dynamic link library (" +
                                        (embeddedPdbPath.empty() ? dllFilesystemPath.filename().string() + ".pdb" : embeddedPdbPath) +
                                        "). Dynamic link library must be built in Debug mode with accessible PDB symbols.";
        return validationResult;
    }

    validationResult.isDebugBuild = true;
    validationResult.hasPdbFile = true;
    validationResult.pdbFilePath = resolvedPdbPath.string();
    return validationResult;
}

static void suspendAllTargetThreads(DWORD targetProcessIdentifier)
{
    HANDLE snapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshotHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    THREADENTRY32 threadEntry = {0};
    threadEntry.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(snapshotHandle, &threadEntry))
    {
        do
        {
            if (threadEntry.th32OwnerProcessID == targetProcessIdentifier)
            {
                HANDLE threadHandle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);
                if (threadHandle != NULL)
                {
                    SuspendThread(threadHandle);
                    CloseHandle(threadHandle);
                }
            }
        } while (Thread32Next(snapshotHandle, &threadEntry));
    }

    CloseHandle(snapshotHandle);
}

bool resumeTargetProcess(DWORD targetProcessIdentifier)
{
    HANDLE snapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshotHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    THREADENTRY32 threadEntry = {0};
    threadEntry.dwSize = sizeof(THREADENTRY32);
    bool resumedAnyThread = false;

    if (Thread32First(snapshotHandle, &threadEntry))
    {
        do
        {
            if (threadEntry.th32OwnerProcessID == targetProcessIdentifier)
            {
                HANDLE threadHandle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);
                if (threadHandle != NULL)
                {
                    DWORD suspendCount = 0;
                    do
                    {
                        suspendCount = ResumeThread(threadHandle);
                    } while (suspendCount > 1 && suspendCount != static_cast<DWORD>(-1));
                    CloseHandle(threadHandle);
                    resumedAnyThread = true;
                }
            }
        } while (Thread32Next(snapshotHandle, &threadEntry));
    }

    CloseHandle(snapshotHandle);
    return resumedAnyThread;
}

bool terminateTargetProcess(DWORD targetProcessIdentifier)
{
    HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, targetProcessIdentifier);
    if (!processHandle)
    {
        return false;
    }
    BOOL terminationSuccess = TerminateProcess(processHandle, 1);
    CloseHandle(processHandle);
    return terminationSuccess == TRUE;
}

bool runCrashDebuggerSession(DWORD targetProcessIdentifier, const std::string &targetModulePath, const std::string &outputDirectoryPath, CrashReportData &outputReport)
{
    std::filesystem::create_directories(outputDirectoryPath);

    if (!DebugActiveProcess(targetProcessIdentifier))
    {
        return false;
    }

    DebugSetProcessKillOnExit(FALSE);

    HANDLE targetProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetProcessIdentifier);
    if (!targetProcessHandle)
    {
        targetProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, targetProcessIdentifier);
    }

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEBUG);

    std::filesystem::path moduleDirectoryPath = std::filesystem::path(targetModulePath).parent_path();
    std::string symbolSearchPath = moduleDirectoryPath.string() + ";" + std::filesystem::current_path().string();
    SymInitialize(targetProcessHandle, symbolSearchPath.c_str(), TRUE);

    bool crashEncountered = false;
    bool keepDebugging = true;
    int initialBreakpointCount = 0;

    while (keepDebugging)
    {
        DEBUG_EVENT debugEvent = {0};
        if (!WaitForDebugEvent(&debugEvent, 200))
        {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(targetProcessHandle, &exitCode) && exitCode != STILL_ACTIVE)
            {
                break;
            }
            continue;
        }

        DWORD continueStatus = DBG_CONTINUE;

        switch (debugEvent.dwDebugEventCode)
        {
        case CREATE_PROCESS_DEBUG_EVENT:
        {
            if (debugEvent.u.CreateProcessInfo.hFile)
            {
                CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
            }
            break;
        }
        case LOAD_DLL_DEBUG_EVENT:
        {
            if (debugEvent.u.LoadDll.hFile)
            {
                CloseHandle(debugEvent.u.LoadDll.hFile);
            }
            break;
        }
        case EXCEPTION_DEBUG_EVENT:
        {
            const EXCEPTION_RECORD &exceptionRecord = debugEvent.u.Exception.ExceptionRecord;
            DWORD exceptionCode = exceptionRecord.ExceptionCode;

            if (exceptionCode == EXCEPTION_BREAKPOINT && initialBreakpointCount == 0)
            {
                initialBreakpointCount++;
                continueStatus = DBG_CONTINUE;
                break;
            }

            bool isFatalCrash = false;
            if (exceptionCode == EXCEPTION_ACCESS_VIOLATION ||
                exceptionCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
                exceptionCode == EXCEPTION_DATATYPE_MISALIGNMENT ||
                exceptionCode == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
                exceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
                exceptionCode == EXCEPTION_IN_PAGE_ERROR ||
                exceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
                exceptionCode == EXCEPTION_PRIV_INSTRUCTION ||
                exceptionCode == EXCEPTION_STACK_OVERFLOW ||
                exceptionCode == 0xC0000374 ||
                debugEvent.u.Exception.dwFirstChance == 0)
            {
                isFatalCrash = true;
            }

            if (isFatalCrash)
            {
                crashEncountered = true;
                keepDebugging = false;

                suspendAllTargetThreads(targetProcessIdentifier);

                std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
                std::string timestamp = formatDateTimeString(currentTime);

                outputReport.targetProcessIdentifier = targetProcessIdentifier;
                outputReport.faultingThreadIdentifier = debugEvent.dwThreadId;
                outputReport.exceptionCode = exceptionCode;
                outputReport.exceptionDescription = describeExceptionCode(exceptionCode);
                outputReport.faultingAddress = reinterpret_cast<DWORD64>(exceptionRecord.ExceptionAddress);
                outputReport.timestampString = timestamp;

                std::filesystem::path dumpPath = std::filesystem::path(outputDirectoryPath) / ("crash_" + std::to_string(targetProcessIdentifier) + "_" + timestamp + ".dmp");
                std::filesystem::path jsonPath = std::filesystem::path(outputDirectoryPath) / ("crash_" + std::to_string(targetProcessIdentifier) + "_" + timestamp + ".json");
                std::filesystem::path logPath = std::filesystem::path(outputDirectoryPath) / ("crash_" + std::to_string(targetProcessIdentifier) + "_" + timestamp + ".log");

                outputReport.minidumpFilePath = dumpPath.string();
                outputReport.reportJsonFilePath = jsonPath.string();
                outputReport.reportLogFilePath = logPath.string();

                HANDLE dumpFileHandle = CreateFileA(dumpPath.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (dumpFileHandle != INVALID_HANDLE_VALUE)
                {
                    MINIDUMP_EXCEPTION_INFORMATION exceptionInformation;
                    exceptionInformation.ThreadId = debugEvent.dwThreadId;
                    EXCEPTION_POINTERS exceptionPointersStructure;
                    EXCEPTION_RECORD localExceptionRecordCopy = exceptionRecord;
                    exceptionPointersStructure.ExceptionRecord = &localExceptionRecordCopy;

                    HANDLE faultingThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, debugEvent.dwThreadId);
                    CONTEXT threadContext = {0};
                    threadContext.ContextFlags = CONTEXT_ALL;
                    GetThreadContext(faultingThreadHandle, &threadContext);
                    exceptionPointersStructure.ContextRecord = &threadContext;

                    exceptionInformation.ExceptionPointers = &exceptionPointersStructure;
                    exceptionInformation.ClientPointers = FALSE;

                    MiniDumpWriteDump(targetProcessHandle, targetProcessIdentifier, dumpFileHandle, MiniDumpWithFullMemory, &exceptionInformation, NULL, NULL);
                    CloseHandle(dumpFileHandle);

                    if (faultingThreadHandle)
                    {
#if defined(_M_AMD64)
                        outputReport.cpuRegisters["RAX"] = formatHexValueString(threadContext.Rax);
                        outputReport.cpuRegisters["RBX"] = formatHexValueString(threadContext.Rbx);
                        outputReport.cpuRegisters["RCX"] = formatHexValueString(threadContext.Rcx);
                        outputReport.cpuRegisters["RDX"] = formatHexValueString(threadContext.Rdx);
                        outputReport.cpuRegisters["RSI"] = formatHexValueString(threadContext.Rsi);
                        outputReport.cpuRegisters["RDI"] = formatHexValueString(threadContext.Rdi);
                        outputReport.cpuRegisters["RSP"] = formatHexValueString(threadContext.Rsp);
                        outputReport.cpuRegisters["RBP"] = formatHexValueString(threadContext.Rbp);
                        outputReport.cpuRegisters["RIP"] = formatHexValueString(threadContext.Rip);
                        outputReport.cpuRegisters["R8"] = formatHexValueString(threadContext.R8);
                        outputReport.cpuRegisters["R9"] = formatHexValueString(threadContext.R9);
                        outputReport.cpuRegisters["R10"] = formatHexValueString(threadContext.R10);
                        outputReport.cpuRegisters["R11"] = formatHexValueString(threadContext.R11);
                        outputReport.cpuRegisters["R12"] = formatHexValueString(threadContext.R12);
                        outputReport.cpuRegisters["R13"] = formatHexValueString(threadContext.R13);
                        outputReport.cpuRegisters["R14"] = formatHexValueString(threadContext.R14);
                        outputReport.cpuRegisters["R15"] = formatHexValueString(threadContext.R15);
                        outputReport.cpuRegisters["EFLAGS"] = formatHexValueString(threadContext.EFlags);

                        STACKFRAME64 stackFrame = {0};
                        stackFrame.AddrPC.Offset = threadContext.Rip;
                        stackFrame.AddrPC.Mode = AddrModeFlat;
                        stackFrame.AddrFrame.Offset = threadContext.Rbp;
                        stackFrame.AddrFrame.Mode = AddrModeFlat;
                        stackFrame.AddrStack.Offset = threadContext.Rsp;
                        stackFrame.AddrStack.Mode = AddrModeFlat;

                        DWORD machineArchitecture = IMAGE_FILE_MACHINE_AMD64;
#else
                        outputReport.cpuRegisters["EAX"] = formatHexValueString(threadContext.Eax);
                        outputReport.cpuRegisters["EBX"] = formatHexValueString(threadContext.Ebx);
                        outputReport.cpuRegisters["ECX"] = formatHexValueString(threadContext.Ecx);
                        outputReport.cpuRegisters["EDX"] = formatHexValueString(threadContext.Edx);
                        outputReport.cpuRegisters["ESI"] = formatHexValueString(threadContext.Esi);
                        outputReport.cpuRegisters["EDI"] = formatHexValueString(threadContext.Edi);
                        outputReport.cpuRegisters["ESP"] = formatHexValueString(threadContext.Esp);
                        outputReport.cpuRegisters["EBP"] = formatHexValueString(threadContext.Ebp);
                        outputReport.cpuRegisters["EIP"] = formatHexValueString(threadContext.Eip);
                        outputReport.cpuRegisters["EFLAGS"] = formatHexValueString(threadContext.EFlags);

                        STACKFRAME64 stackFrame = {0};
                        stackFrame.AddrPC.Offset = threadContext.Eip;
                        stackFrame.AddrPC.Mode = AddrModeFlat;
                        stackFrame.AddrFrame.Offset = threadContext.Ebp;
                        stackFrame.AddrFrame.Mode = AddrModeFlat;
                        stackFrame.AddrStack.Offset = threadContext.Esp;
                        stackFrame.AddrStack.Mode = AddrModeFlat;

                        DWORD machineArchitecture = IMAGE_FILE_MACHINE_I386;
#endif

                        int frameCounter = 0;
                        while (StackWalk64(machineArchitecture, targetProcessHandle, faultingThreadHandle, &stackFrame, &threadContext, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
                        {
                            if (stackFrame.AddrPC.Offset == 0)
                            {
                                break;
                            }

                            StackFrameInformation singleFrame;
                            singleFrame.frameIndex = frameCounter++;
                            singleFrame.instructionOffset = stackFrame.AddrPC.Offset;

                            IMAGEHLP_MODULE64 moduleInformation = {0};
                            moduleInformation.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
                            if (SymGetModuleInfo64(targetProcessHandle, stackFrame.AddrPC.Offset, &moduleInformation))
                            {
                                singleFrame.moduleName = moduleInformation.ModuleName;
                            }

                            char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {0};
                            PSYMBOL_INFO symbolPointer = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
                            symbolPointer->SizeOfStruct = sizeof(SYMBOL_INFO);
                            symbolPointer->MaxNameLen = MAX_SYM_NAME;
                            DWORD64 symbolDisplacement = 0;

                            if (SymFromAddr(targetProcessHandle, stackFrame.AddrPC.Offset, &symbolDisplacement, symbolPointer))
                            {
                                singleFrame.functionName = symbolPointer->Name;
                                singleFrame.displacement = symbolDisplacement;
                            }

                            IMAGEHLP_LINE64 lineInformation = {0};
                            lineInformation.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                            DWORD lineDisplacement = 0;
                            if (SymGetLineFromAddr64(targetProcessHandle, stackFrame.AddrPC.Offset, &lineDisplacement, &lineInformation))
                            {
                                singleFrame.sourceFilePath = lineInformation.FileName ? lineInformation.FileName : "";
                                singleFrame.sourceLineNumber = lineInformation.LineNumber;
                            }

                            IMAGEHLP_STACK_FRAME imageStackFrame = {0};
                            imageStackFrame.InstructionOffset = stackFrame.AddrPC.Offset;
                            if (SymSetContext(targetProcessHandle, &imageStackFrame, NULL))
                            {
                                SymbolEnumerationContext enumContext;
                                enumContext.targetProcessHandle = targetProcessHandle;
                                enumContext.currentStackFrame = stackFrame;
                                enumContext.variableCollection = &singleFrame.localVariables;

                                SymEnumSymbols(targetProcessHandle, 0, "*", EnumerateSymbolsCallback, &enumContext);
                            }

                            outputReport.callStackTrace.push_back(singleFrame);
                        }

                        CloseHandle(faultingThreadHandle);
                    }
                }

                JsonDocument jsonReport;
                jsonReport["processIdentifier"] = outputReport.targetProcessIdentifier;
                jsonReport["faultingThreadIdentifier"] = outputReport.faultingThreadIdentifier;
                jsonReport["exceptionCode"] = formatHexValueString(outputReport.exceptionCode);
                jsonReport["exceptionDescription"] = outputReport.exceptionDescription;
                jsonReport["faultingAddress"] = formatHexValueString(outputReport.faultingAddress);
                jsonReport["timestamp"] = outputReport.timestampString;
                jsonReport["minidumpPath"] = outputReport.minidumpFilePath;
                jsonReport["registers"] = outputReport.cpuRegisters;

                JsonDocument stackArray = JsonDocument::array();
                for (const auto &frame : outputReport.callStackTrace)
                {
                    JsonDocument frameJson;
                    frameJson["frameIndex"] = frame.frameIndex;
                    frameJson["instructionOffset"] = formatHexValueString(frame.instructionOffset);
                    frameJson["module"] = frame.moduleName;
                    frameJson["function"] = frame.functionName;
                    frameJson["sourceFile"] = frame.sourceFilePath;
                    frameJson["lineNumber"] = frame.sourceLineNumber;
                    frameJson["displacement"] = frame.displacement;

                    JsonDocument variablesArray = JsonDocument::array();
                    for (const auto &variable : frame.localVariables)
                    {
                        JsonDocument variableJson;
                        variableJson["name"] = variable.variableName;
                        variableJson["type"] = variable.typeName;
                        variableJson["address"] = formatHexValueString(variable.address);
                        variableJson["value"] = variable.valueRepresentation;
                        variablesArray.push_back(variableJson);
                    }
                    frameJson["variables"] = variablesArray;
                    stackArray.push_back(frameJson);
                }
                jsonReport["callStack"] = stackArray;

                std::ofstream jsonOutputFile(jsonPath);
                if (jsonOutputFile.is_open())
                {
                    jsonOutputFile << jsonReport.dump(2);
                }

                std::ofstream textLogFile(logPath);
                if (textLogFile.is_open())
                {
                    textLogFile << "================================================================================\n";
                    textLogFile << "                     MOON INJECTOR CRASH DUMP TRACE REPORT                      \n";
                    textLogFile << "================================================================================\n";
                    textLogFile << "Process ID:          " << outputReport.targetProcessIdentifier << "\n";
                    textLogFile << "Thread ID:           " << outputReport.faultingThreadIdentifier << "\n";
                    textLogFile << "Exception Code:      " << formatHexValueString(outputReport.exceptionCode) << " (" << outputReport.exceptionDescription << ")\n";
                    textLogFile << "Fault Address:       " << formatHexValueString(outputReport.faultingAddress) << "\n";
                    textLogFile << "Timestamp:           " << outputReport.timestampString << "\n";
                    textLogFile << "Minidump File:       " << outputReport.minidumpFilePath << "\n\n";

                    textLogFile << "CPU REGISTERS:\n";
                    for (const auto &registerEntry : outputReport.cpuRegisters)
                    {
                        textLogFile << "  " << registerEntry.first << " = " << registerEntry.second << "\n";
                    }

                    textLogFile << "\nCALL STACK TRACE TREE:\n";
                    for (const auto &frame : outputReport.callStackTrace)
                    {
                        textLogFile << "  #" << std::setw(2) << frame.frameIndex << "  "
                                    << (frame.moduleName.empty() ? "unknown" : frame.moduleName) << "!"
                                    << (frame.functionName.empty() ? formatHexValueString(frame.instructionOffset) : frame.functionName)
                                    << " + " << formatHexValueString(frame.displacement) << "\n";

                        if (!frame.sourceFilePath.empty())
                        {
                            textLogFile << "       Source: " << frame.sourceFilePath << ":" << frame.sourceLineNumber << "\n";
                        }

                        if (!frame.localVariables.empty())
                        {
                            textLogFile << "       Variables:\n";
                            for (const auto &variable : frame.localVariables)
                            {
                                textLogFile << "         * " << variable.variableName << " (" << variable.typeName << ") = " << variable.valueRepresentation << "\n";
                            }
                        }
                    }
                    textLogFile << "\n================================================================================\n";
                    textLogFile << "Target process suspended and kept in memory for inspection.\n";
                }

                DebugActiveProcessStop(targetProcessIdentifier);
                continueStatus = DBG_CONTINUE;
                break;
            }

            continueStatus = DBG_EXCEPTION_NOT_HANDLED;
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
        {
            keepDebugging = false;
            break;
        }
        default:
            break;
        }

        ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
    }

    SymCleanup(targetProcessHandle);
    if (targetProcessHandle)
    {
        CloseHandle(targetProcessHandle);
    }

    return crashEncountered;
}

std::vector<std::string> listAvailableCrashReports(const std::string &reportsDirectoryPath)
{
    std::vector<std::string> foundReports;
    if (!std::filesystem::exists(reportsDirectoryPath))
    {
        return foundReports;
    }

    for (const auto &directoryEntry : std::filesystem::directory_iterator(reportsDirectoryPath))
    {
        if (directoryEntry.is_regular_file() && directoryEntry.path().extension() == ".json")
        {
            foundReports.push_back(directoryEntry.path().string());
        }
    }
    return foundReports;
}
