#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <map>

struct DebugInformationValidationResult
{
    bool isDebugBuild = false;
    bool hasPdbFile = false;
    std::string pdbFilePath = "";
    std::string errorMessage = "";
};

struct VariableInformation
{
    std::string variableName = "";
    std::string typeName = "";
    DWORD64 address = 0;
    DWORD size = 0;
    std::string valueRepresentation = "";
};

struct StackFrameInformation
{
    int frameIndex = 0;
    std::string moduleName = "";
    std::string functionName = "";
    std::string sourceFilePath = "";
    int sourceLineNumber = 0;
    DWORD64 displacement = 0;
    DWORD64 instructionOffset = 0;
    std::vector<VariableInformation> localVariables;
};

struct CrashReportData
{
    DWORD targetProcessIdentifier = 0;
    DWORD faultingThreadIdentifier = 0;
    DWORD exceptionCode = 0;
    std::string exceptionDescription = "";
    DWORD64 faultingAddress = 0;
    std::string timestampString = "";
    std::string minidumpFilePath = "";
    std::string reportJsonFilePath = "";
    std::string reportLogFilePath = "";
    std::map<std::string, std::string> cpuRegisters;
    std::vector<StackFrameInformation> callStackTrace;
};

DebugInformationValidationResult validateDynamicLinkLibraryDebugInfo(const std::string &dynamicLinkLibraryPath);

bool runCrashDebuggerSession(DWORD targetProcessIdentifier, const std::string &targetModulePath, const std::string &outputDirectoryPath, CrashReportData &outputReport);

bool resumeTargetProcess(DWORD targetProcessIdentifier);

bool terminateTargetProcess(DWORD targetProcessIdentifier);

std::vector<std::string> listAvailableCrashReports(const std::string &reportsDirectoryPath);
