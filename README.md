# Moon-Injector

![image](./mooninjector.png)


you have to compile blackbone get the static library "BlackBone.lib" and put next to .cpp files. The lib is huge like 90 mb

project structure:
Moon Injector/
|-- |-- BlackBone/
|-- |   |-- .gitattributes
|-- |   |-- .gitignore
|-- |   |-- BlackBone.sln
|-- |   |-- DIA/
|-- |   |   |-- Win32/
|-- |   |   |   |-- msdia140.dll
|-- |   |   |   |-- symsrv.dll
|-- |   |   |-- x64/
|-- |   |   |   |-- msdia140.dll
|-- |   |   |   |-- symsrv.dll
|-- |   |-- LICENSE
|-- |   |-- README.md
|-- |   |-- Testing/
|-- |   |   |-- TestDll32.dll
|-- |   |   |-- TestDll64.dll
|-- |   |   |-- TestHelper32.exe
|-- |   |   |-- TestHelper64.exe
|-- |   |-- obj/
|-- |   |   |-- x64/
|-- |   |   |   |-- Release/
|-- |   |   |   |   |-- BlackBone/
|-- |   |   |   |   |   |-- AsmHelper32.obj
|-- |   |   |   |   |   |-- AsmHelper64.obj
|-- |   |   |   |   |   |-- BlackBone.lib.recipe
|-- |   |   |   |   |   |-- BlackBone.log
|-- |   |   |   |   |   |-- BlackBone.tlog/
|-- |   |   |   |   |   |   |-- BlackBone.lastbuildstate
|-- |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |-- Lib-link.read.1.tlog
|-- |   |   |   |   |   |   |-- Lib-link.write.1.tlog
|-- |   |   |   |   |   |   |-- Lib.command.1.tlog
|-- |   |   |   |   |   |   |-- Masm.read.1u.tlog
|-- |   |   |   |   |   |   |-- Masm.write.1u.tlog
|-- |   |   |   |   |   |-- BlackBone_MD.tlog/
|-- |   |   |   |   |   |-- DriverControl.obj
|-- |   |   |   |   |   |-- ImageNET.obj
|-- |   |   |   |   |   |-- InitOnce.obj
|-- |   |   |   |   |   |-- LDasm.obj
|-- |   |   |   |   |   |-- LocalHookBase.obj
|-- |   |   |   |   |   |-- MExcept.obj
|-- |   |   |   |   |   |-- MMap.obj
|-- |   |   |   |   |   |-- MemBlock.obj
|-- |   |   |   |   |   |-- NameResolve.obj
|-- |   |   |   |   |   |-- NativeSubsystem.obj
|-- |   |   |   |   |   |-- NtLoader.obj
|-- |   |   |   |   |   |-- PDBHelper.obj
|-- |   |   |   |   |   |-- PEImage.obj
|-- |   |   |   |   |   |-- PatternLoader.obj
|-- |   |   |   |   |   |-- PatternSearch.obj
|-- |   |   |   |   |   |-- Process.obj
|-- |   |   |   |   |   |-- ProcessCore.obj
|-- |   |   |   |   |   |-- ProcessMemory.obj
|-- |   |   |   |   |   |-- ProcessModules.obj
|-- |   |   |   |   |   |-- RemoteExec.obj
|-- |   |   |   |   |   |-- RemoteHook.obj
|-- |   |   |   |   |   |-- RemoteLocalHook.obj
|-- |   |   |   |   |   |-- RemoteMemory.obj
|-- |   |   |   |   |   |-- SymbolData.obj
|-- |   |   |   |   |   |-- SymbolLoader.obj
|-- |   |   |   |   |   |-- Syscall64.obj
|-- |   |   |   |   |   |-- Thread.obj
|-- |   |   |   |   |   |-- Threads.obj
|-- |   |   |   |   |   |-- TraceHook.obj
|-- |   |   |   |   |   |-- Utils.obj
|-- |   |   |   |   |   |-- Wow64Subsystem.obj
|-- |   |   |   |   |   |-- assembler.obj
|-- |   |   |   |   |   |-- codegen.obj
|-- |   |   |   |   |   |-- constpool.obj
|-- |   |   |   |   |   |-- containers.obj
|-- |   |   |   |   |   |-- cpuinfo.obj
|-- |   |   |   |   |   |-- cputicks.obj
|-- |   |   |   |   |   |-- error.obj
|-- |   |   |   |   |   |-- globals.obj
|-- |   |   |   |   |   |-- microsoft/
|-- |   |   |   |   |   |   |-- STL/
|-- |   |   |   |   |   |-- operand.obj
|-- |   |   |   |   |   |-- runtime.obj
|-- |   |   |   |   |   |-- string.obj
|-- |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |   |-- vmem.obj
|-- |   |   |   |   |   |-- wow64ext.obj
|-- |   |   |   |   |   |-- x86Subsystem.obj
|-- |   |   |   |   |   |-- x86assembler.obj
|-- |   |   |   |   |   |-- x86cpuinfo.obj
|-- |   |   |   |   |   |-- x86inst.obj
|-- |   |   |   |   |   |-- x86operand.obj
|-- |   |   |   |   |   |-- x86operand_regs.obj
|-- |   |   |   |   |   |-- zone.obj
|-- |   |   |   |   |-- BlackboneTest/
|-- |   |   |   |   |   |-- BlackBoneTest.log
|-- |   |   |   |   |   |-- BlackboneTest.dll.recipe
|-- |   |   |   |   |   |-- BlackboneTest.iobj
|-- |   |   |   |   |   |-- BlackboneTest.ipdb
|-- |   |   |   |   |   |-- BlackboneTest.tlog/
|-- |   |   |   |   |   |   |-- BlackboneTest.lastbuildstate
|-- |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |   |   |-- BlackboneTest_MD.tlog/
|-- |   |   |   |   |   |-- TestAsmJit.obj
|-- |   |   |   |   |   |-- TestAsmVariant.obj
|-- |   |   |   |   |   |-- TestBasic.obj
|-- |   |   |   |   |   |-- TestDriver.obj
|-- |   |   |   |   |   |-- TestGuard.obj
|-- |   |   |   |   |   |-- TestLocalHook.obj
|-- |   |   |   |   |   |-- TestManualMap.obj
|-- |   |   |   |   |   |-- TestModules.obj
|-- |   |   |   |   |   |-- TestMultiPtr.obj
|-- |   |   |   |   |   |-- TestPatternScan.obj
|-- |   |   |   |   |   |-- TestRemoteCall.obj
|-- |   |   |   |   |   |-- TestRemoteHook.obj
|-- |   |   |   |   |   |-- TestRemoteMemory.obj
|-- |   |   |   |   |   |-- TestSymbols.obj
|-- |   |   |   |   |   |-- TestSyscall.obj
|-- |   |   |   |   |   |-- microsoft/
|-- |   |   |   |   |   |   |-- STL/
|-- |   |   |   |   |   |-- vc143.pdb
|-- |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |-- Samples/
|-- |   |   |   |   |   |-- Main.obj
|-- |   |   |   |   |   |-- ManualMap.obj
|-- |   |   |   |   |   |-- Samples.log
|-- |   |   |   |   |   |-- Samples.tlog/
|-- |   |   |   |   |   |   |-- CL.5444.write.1.tlog
|-- |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |-- Samples.lastbuildstate
|-- |   |   |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |   |   |-- Samples64.exe.recipe
|-- |   |   |   |   |   |-- Samples64.iobj
|-- |   |   |   |   |   |-- Samples64.ipdb
|-- |   |   |   |   |   |-- Samples_MD.tlog/
|-- |   |   |   |   |   |-- microsoft/
|-- |   |   |   |   |   |   |-- STL/
|-- |   |   |   |   |   |-- vc143.pdb
|-- |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |-- src/
|-- |   |   |-- 3rd_party/
|-- |   |   |   |-- AsmJit/
|-- |   |   |   |   |-- ApiBegin.h
|-- |   |   |   |   |-- ApiEnd.h
|-- |   |   |   |   |-- AsmJit.h
|-- |   |   |   |   |-- Build.h
|-- |   |   |   |   |-- Config.h
|-- |   |   |   |   |-- LICENSE.md
|-- |   |   |   |   |-- README.md
|-- |   |   |   |   |-- base/
|-- |   |   |   |   |   |-- assembler.cpp
|-- |   |   |   |   |   |-- assembler.h
|-- |   |   |   |   |   |-- codegen.cpp
|-- |   |   |   |   |   |-- codegen.h
|-- |   |   |   |   |   |-- compiler.cpp
|-- |   |   |   |   |   |-- compiler.h
|-- |   |   |   |   |   |-- constpool.cpp
|-- |   |   |   |   |   |-- constpool.h
|-- |   |   |   |   |   |-- containers.cpp
|-- |   |   |   |   |   |-- containers.h
|-- |   |   |   |   |   |-- context.cpp
|-- |   |   |   |   |   |-- context_p.h
|-- |   |   |   |   |   |-- cpuinfo.cpp
|-- |   |   |   |   |   |-- cpuinfo.h
|-- |   |   |   |   |   |-- cputicks.cpp
|-- |   |   |   |   |   |-- cputicks.h
|-- |   |   |   |   |   |-- error.cpp
|-- |   |   |   |   |   |-- error.h
|-- |   |   |   |   |   |-- globals.cpp
|-- |   |   |   |   |   |-- globals.h
|-- |   |   |   |   |   |-- intutil.cpp
|-- |   |   |   |   |   |-- intutil.h
|-- |   |   |   |   |   |-- lock.h
|-- |   |   |   |   |   |-- logger.cpp
|-- |   |   |   |   |   |-- logger.h
|-- |   |   |   |   |   |-- operand.cpp
|-- |   |   |   |   |   |-- operand.h
|-- |   |   |   |   |   |-- runtime.cpp
|-- |   |   |   |   |   |-- runtime.h
|-- |   |   |   |   |   |-- string.cpp
|-- |   |   |   |   |   |-- string.h
|-- |   |   |   |   |   |-- vectypes.h
|-- |   |   |   |   |   |-- vmem.cpp
|-- |   |   |   |   |   |-- vmem.h
|-- |   |   |   |   |   |-- zone.cpp
|-- |   |   |   |   |   |-- zone.h
|-- |   |   |   |   |-- base.h
|-- |   |   |   |   |-- host.h
|-- |   |   |   |   |-- x86/
|-- |   |   |   |   |   |-- x86assembler.cpp
|-- |   |   |   |   |   |-- x86assembler.h
|-- |   |   |   |   |   |-- x86compiler.cpp
|-- |   |   |   |   |   |-- x86compiler.h
|-- |   |   |   |   |   |-- x86context.cpp
|-- |   |   |   |   |   |-- x86context_p.h
|-- |   |   |   |   |   |-- x86cpuinfo.cpp
|-- |   |   |   |   |   |-- x86cpuinfo.h
|-- |   |   |   |   |   |-- x86inst.cpp
|-- |   |   |   |   |   |-- x86inst.h
|-- |   |   |   |   |   |-- x86operand.cpp
|-- |   |   |   |   |   |-- x86operand.h
|-- |   |   |   |   |   |-- x86operand_regs.cpp
|-- |   |   |   |   |   |-- x86scheduler.cpp
|-- |   |   |   |   |   |-- x86scheduler_p.h
|-- |   |   |   |   |-- x86.h
|-- |   |   |   |-- BeaEngine/
|-- |   |   |   |   |-- Warning_for_C_coders.txt
|-- |   |   |   |   |-- Win32/
|-- |   |   |   |   |   |-- Dll/
|-- |   |   |   |   |   |   |-- BeaEngine.dll
|-- |   |   |   |   |   |   |-- BeaEngine.exp
|-- |   |   |   |   |   |   |-- BeaEngine.lib
|-- |   |   |   |   |   |   |-- BeaEngineCheetah.dll
|-- |   |   |   |   |   |   |-- BeaEngineCheetah.exp
|-- |   |   |   |   |   |   |-- BeaEngineCheetah.lib
|-- |   |   |   |   |   |-- Lib/
|-- |   |   |   |   |   |   |-- BeaEngine.lib
|-- |   |   |   |   |   |   |-- BeaEngineCheetah.lib
|-- |   |   |   |   |-- Win64/
|-- |   |   |   |   |   |-- Dll/
|-- |   |   |   |   |   |   |-- BeaEngine64.dll
|-- |   |   |   |   |   |   |-- BeaEngine64.exp
|-- |   |   |   |   |   |   |-- BeaEngine64.lib
|-- |   |   |   |   |   |   |-- BeaEngineCheetah64.dll
|-- |   |   |   |   |   |   |-- BeaEngineCheetah64.exp
|-- |   |   |   |   |   |   |-- BeaEngineCheetah64.lib
|-- |   |   |   |   |   |-- Lib/
|-- |   |   |   |   |   |   |-- BeaEngine64.lib
|-- |   |   |   |   |   |   |-- BeaEngineCheetah64.lib
|-- |   |   |   |   |-- headers/
|-- |   |   |   |   |   |-- BeaEngine.h
|-- |   |   |   |   |   |-- Includes/
|-- |   |   |   |   |   |   |-- basic_types.h
|-- |   |   |   |   |   |   |-- export.h
|-- |   |   |   |-- CorError.h
|-- |   |   |   |-- DIA/
|-- |   |   |   |   |-- cvconst.h
|-- |   |   |   |   |-- dia2.h
|-- |   |   |   |   |-- diacreate.h
|-- |   |   |   |   |-- lib/
|-- |   |   |   |   |   |-- amd64/
|-- |   |   |   |   |   |   |-- diaguids.lib
|-- |   |   |   |   |   |-- diaguids.lib
|-- |   |   |   |-- VersionApi.h
|-- |   |   |   |-- cor.h
|-- |   |   |   |-- corhdr.h
|-- |   |   |   |-- gchost.h
|-- |   |   |   |-- ivalidator.h
|-- |   |   |   |-- ivehandler.h
|-- |   |   |   |-- metahost.h
|-- |   |   |   |-- mscoree.h
|-- |   |   |   |-- rewolf-wow64ext/
|-- |   |   |   |   |-- .gitignore
|-- |   |   |   |   |-- .hgignore
|-- |   |   |   |   |-- README.md
|-- |   |   |   |   |-- doc/
|-- |   |   |   |   |   |-- wow64ext.txt
|-- |   |   |   |   |-- lgpl-3.0.txt
|-- |   |   |   |   |-- sample/
|-- |   |   |   |   |   |-- build.bat
|-- |   |   |   |   |   |-- main.cpp
|-- |   |   |   |   |-- src/
|-- |   |   |   |   |   |-- CMemPtr.h
|-- |   |   |   |   |   |-- internal.h
|-- |   |   |   |   |   |-- resource.h
|-- |   |   |   |   |   |-- wow64ext.cpp
|-- |   |   |   |   |   |-- wow64ext.h
|-- |   |   |   |   |   |-- wow64ext.rc
|-- |   |   |   |   |   |-- wow64ext.sln
|-- |   |   |   |   |   |-- wow64ext.vcxproj
|-- |   |   |   |-- winapifamily.h
|-- |   |   |   |-- winpackagefamily.h
|-- |   |   |-- BlackBone/
|-- |   |   |   |-- Asm/
|-- |   |   |   |   |-- AsmFactory.h
|-- |   |   |   |   |-- AsmHelper32.cpp
|-- |   |   |   |   |-- AsmHelper32.h
|-- |   |   |   |   |-- AsmHelper64.cpp
|-- |   |   |   |   |-- AsmHelper64.h
|-- |   |   |   |   |-- AsmStack.hpp
|-- |   |   |   |   |-- AsmVariant.hpp
|-- |   |   |   |   |-- IAsmHelper.h
|-- |   |   |   |   |-- LDasm.c
|-- |   |   |   |   |-- LDasm.h
|-- |   |   |   |-- BlackBone.vcxproj
|-- |   |   |   |-- BlackBone.vcxproj.filters
|-- |   |   |   |-- BlackBone.vcxproj.user
|-- |   |   |   |-- CMakeLists.txt
|-- |   |   |   |-- Config.h
|-- |   |   |   |-- DllMain.cpp
|-- |   |   |   |-- DriverControl/
|-- |   |   |   |   |-- DriverControl.cpp
|-- |   |   |   |   |-- DriverControl.h
|-- |   |   |   |-- Exports.def
|-- |   |   |   |-- Include/
|-- |   |   |   |   |-- ApiSet.h
|-- |   |   |   |   |-- CallResult.h
|-- |   |   |   |   |-- FunctionTypes.h
|-- |   |   |   |   |-- HandleGuard.h
|-- |   |   |   |   |-- Macro.h
|-- |   |   |   |   |-- NativeEnums.h
|-- |   |   |   |   |-- NativeStructures.h
|-- |   |   |   |   |-- Types.h
|-- |   |   |   |   |-- Win7Specific.h
|-- |   |   |   |   |-- Win8Specific.h
|-- |   |   |   |   |-- WinXPSpecific.h
|-- |   |   |   |   |-- Winheaders.h
|-- |   |   |   |-- LocalHook/
|-- |   |   |   |   |-- HookHandlerCdecl.h
|-- |   |   |   |   |-- HookHandlerFastcall.h
|-- |   |   |   |   |-- HookHandlerStdcall.h
|-- |   |   |   |   |-- HookHandlerThiscall.h
|-- |   |   |   |   |-- HookHandlers.h
|-- |   |   |   |   |-- LocalHook.hpp
|-- |   |   |   |   |-- LocalHookBase.cpp
|-- |   |   |   |   |-- LocalHookBase.h
|-- |   |   |   |   |-- TraceHook.cpp
|-- |   |   |   |   |-- TraceHook.h
|-- |   |   |   |   |-- VTableHook.hpp
|-- |   |   |   |-- ManualMap/
|-- |   |   |   |   |-- MExcept.cpp
|-- |   |   |   |   |-- MExcept.h
|-- |   |   |   |   |-- MMap.cpp
|-- |   |   |   |   |-- MMap.h
|-- |   |   |   |   |-- Native/
|-- |   |   |   |   |   |-- NtLoader.cpp
|-- |   |   |   |   |   |-- NtLoader.h
|-- |   |   |   |-- Misc/
|-- |   |   |   |   |-- DynImport.h
|-- |   |   |   |   |-- InitOnce.cpp
|-- |   |   |   |   |-- InitOnce.h
|-- |   |   |   |   |-- NameResolve.cpp
|-- |   |   |   |   |-- NameResolve.h
|-- |   |   |   |   |-- Thunk.hpp
|-- |   |   |   |   |-- Trace.hpp
|-- |   |   |   |   |-- Utils.cpp
|-- |   |   |   |   |-- Utils.h
|-- |   |   |   |-- PE/
|-- |   |   |   |   |-- ImageNET.cpp
|-- |   |   |   |   |-- ImageNET.h
|-- |   |   |   |   |-- PEImage.cpp
|-- |   |   |   |   |-- PEImage.h
|-- |   |   |   |-- Patterns/
|-- |   |   |   |   |-- PatternSearch.cpp
|-- |   |   |   |   |-- PatternSearch.h
|-- |   |   |   |-- Process/
|-- |   |   |   |   |-- MemBlock.cpp
|-- |   |   |   |   |-- MemBlock.h
|-- |   |   |   |   |-- MultPtr.hpp
|-- |   |   |   |   |-- Process.cpp
|-- |   |   |   |   |-- Process.h
|-- |   |   |   |   |-- ProcessCore.cpp
|-- |   |   |   |   |-- ProcessCore.h
|-- |   |   |   |   |-- ProcessMemory.cpp
|-- |   |   |   |   |-- ProcessMemory.h
|-- |   |   |   |   |-- ProcessModules.cpp
|-- |   |   |   |   |-- ProcessModules.h
|-- |   |   |   |   |-- RPC/
|-- |   |   |   |   |   |-- RemoteContext.hpp
|-- |   |   |   |   |   |-- RemoteExec.cpp
|-- |   |   |   |   |   |-- RemoteExec.h
|-- |   |   |   |   |   |-- RemoteFunction.hpp
|-- |   |   |   |   |   |-- RemoteHook.cpp
|-- |   |   |   |   |   |-- RemoteHook.h
|-- |   |   |   |   |   |-- RemoteLocalHook.cpp
|-- |   |   |   |   |   |-- RemoteLocalHook.h
|-- |   |   |   |   |   |-- RemoteMemory.cpp
|-- |   |   |   |   |   |-- RemoteMemory.h
|-- |   |   |   |   |-- Threads/
|-- |   |   |   |   |   |-- Thread.cpp
|-- |   |   |   |   |   |-- Thread.h
|-- |   |   |   |   |   |-- Threads.cpp
|-- |   |   |   |   |   |-- Threads.h
|-- |   |   |   |-- Subsystem/
|-- |   |   |   |   |-- NativeSubsystem.cpp
|-- |   |   |   |   |-- NativeSubsystem.h
|-- |   |   |   |   |-- Wow64Subsystem.cpp
|-- |   |   |   |   |-- Wow64Subsystem.h
|-- |   |   |   |   |-- x86Subsystem.cpp
|-- |   |   |   |   |-- x86Subsystem.h
|-- |   |   |   |-- Symbols/
|-- |   |   |   |   |-- PDBHelper.cpp
|-- |   |   |   |   |-- PDBHelper.h
|-- |   |   |   |   |-- PatternLoader.cpp
|-- |   |   |   |   |-- PatternLoader.h
|-- |   |   |   |   |-- SymbolData.cpp
|-- |   |   |   |   |-- SymbolData.h
|-- |   |   |   |   |-- SymbolLoader.cpp
|-- |   |   |   |   |-- SymbolLoader.h
|-- |   |   |   |-- Syscalls/
|-- |   |   |   |   |-- Syscall.h
|-- |   |   |   |   |-- Syscall32.asm
|-- |   |   |   |   |-- Syscall64.asm
|-- |   |   |-- BlackBoneDrv/
|-- |   |   |   |-- BlackBoneDef.h
|-- |   |   |   |-- BlackBoneDrv.c
|-- |   |   |   |-- BlackBoneDrv.h
|-- |   |   |   |-- BlackBoneDrv.sln
|-- |   |   |   |-- BlackBoneDrv.vcxproj
|-- |   |   |   |-- BlackBoneDrv.vcxproj.filters
|-- |   |   |   |-- Dispatch.c
|-- |   |   |   |-- Imports.h
|-- |   |   |   |-- Inject.c
|-- |   |   |   |-- Loader.c
|-- |   |   |   |-- Loader.h
|-- |   |   |   |-- MMap.c
|-- |   |   |   |-- NativeEnums.h
|-- |   |   |   |-- NativeStructs.h
|-- |   |   |   |-- NativeStructs10.h
|-- |   |   |   |-- NativeStructs7.h
|-- |   |   |   |-- NativeStructs8.h
|-- |   |   |   |-- NativeStructs81.h
|-- |   |   |   |-- NotifyRoutine.c
|-- |   |   |   |-- PEStructs.h
|-- |   |   |   |-- Private.c
|-- |   |   |   |-- Private.h
|-- |   |   |   |-- Remap.c
|-- |   |   |   |-- Remap.h
|-- |   |   |   |-- Routines.c
|-- |   |   |   |-- Routines.h
|-- |   |   |   |-- Utils.c
|-- |   |   |   |-- Utils.h
|-- |   |   |   |-- VadHelpers.c
|-- |   |   |   |-- VadHelpers.h
|-- |   |   |   |-- VadRoutines.c
|-- |   |   |   |-- VadRoutines.h
|-- |   |   |   |-- apiset.h
|-- |   |   |   |-- bin/
|-- |   |   |   |   |-- x64/
|-- |   |   |   |   |   |-- Win10Release/
|-- |   |   |   |   |   |   |-- BlackBoneDrv10.sys
|-- |   |   |   |   |   |-- Win7Release/
|-- |   |   |   |   |   |   |-- BlackBoneDrv7.sys
|-- |   |   |   |   |   |-- Win8.1Release/
|-- |   |   |   |   |   |   |-- BlackBoneDrv81.sys
|-- |   |   |   |   |   |-- Win8Release/
|-- |   |   |   |   |   |   |-- BlackBoneDrv8.sys
|-- |   |   |   |-- ldrreloc.c
|-- |   |   |-- BlackBoneTest/
|-- |   |   |   |-- BlackBoneTest.vcxproj
|-- |   |   |   |-- BlackBoneTest.vcxproj.filters
|-- |   |   |   |-- Common.h
|-- |   |   |   |-- TestAsmJit.cpp
|-- |   |   |   |-- TestAsmVariant.cpp
|-- |   |   |   |-- TestBasic.cpp
|-- |   |   |   |-- TestDriver.cpp
|-- |   |   |   |-- TestGuard.cpp
|-- |   |   |   |-- TestLocalHook.cpp
|-- |   |   |   |-- TestManualMap.cpp
|-- |   |   |   |-- TestModules.cpp
|-- |   |   |   |-- TestMultiPtr.cpp
|-- |   |   |   |-- TestPatternScan.cpp
|-- |   |   |   |-- TestRemoteCall.cpp
|-- |   |   |   |-- TestRemoteHook.cpp
|-- |   |   |   |-- TestRemoteMemory.cpp
|-- |   |   |   |-- TestSymbols.cpp
|-- |   |   |   |-- TestSyscall.cpp
|-- |   |   |-- CMakeLists.txt
|-- |   |   |-- PythonicBlackBone/
|-- |   |   |   |-- BlackBone.py
|-- |   |   |   |-- __pycache__/
|-- |   |   |   |   |-- BlackBone.cpython-38.pyc
|-- |   |   |   |-- test/
|-- |   |   |   |   |-- ReadWriteProcMem.py
|-- |   |   |-- Samples/
|-- |   |   |   |-- CMakeLists.txt
|-- |   |   |   |-- Main.cpp
|-- |   |   |   |-- ManualMap.cpp
|-- |   |   |   |-- Samples.vcxproj
|-- |   |   |   |-- Samples.vcxproj.filters
|-- |-- BlackBone.lib
|-- |-- CMakeLists.txt
|-- |-- THE_MOON/
|-- |   |-- .vs/
|-- |   |   |-- MoonInjector/
|-- |   |   |   |-- CopilotIndices/
|-- |   |   |   |   |-- 17.14.827.52834/
|-- |   |   |   |   |   |-- CodeChunks.db
|-- |   |   |   |   |   |-- SemanticSymbols.db
|-- |   |   |   |-- FileContentIndex/
|-- |   |   |   |   |-- 06207a36-8a62-4c89-91e9-c3240af6e587.vsidx
|-- |   |   |   |   |-- 8b1359b3-8034-4b79-8a55-d274ec1f8355.vsidx
|-- |   |   |   |-- copilot-chat/
|-- |   |   |   |   |-- 10abab84/
|-- |   |   |   |   |   |-- sessions/
|-- |   |   |   |-- v17/
|-- |   |   |   |   |-- .suo
|-- |   |   |   |   |-- Browse.VC.db
|-- |   |   |   |   |-- DocumentLayout.backup.json
|-- |   |   |   |   |-- DocumentLayout.json
|-- |   |   |   |   |-- Solution.VC.db
|-- |   |   |   |   |-- Solution.VC.db-shm
|-- |   |   |   |   |-- Solution.VC.db-wal
|-- |   |   |   |   |-- fileList.bin
|-- |   |   |   |   |-- ipch/
|-- |   |   |   |   |   |-- AutoPCH/
|-- |   |   |   |   |   |   |-- 2ea70000602dcb2f/
|-- |   |   |   |   |   |   |   |-- INJECTOR.ipch
|-- |   |   |   |   |   |   |-- 434b845215e36d/
|-- |   |   |   |   |   |   |   |-- INJECTOR.ipch
|-- |   |   |   |   |   |   |-- 48c17d1fe2a11d1a/
|-- |   |   |   |   |   |   |   |-- INITONCE.ipch
|-- |   |   |   |   |   |   |-- 5627f94e0f8e638a/
|-- |   |   |   |   |   |   |   |-- MAIN.ipch
|-- |   |   |   |   |   |   |-- 7baad474833d3fc3/
|-- |   |   |   |   |   |   |   |-- MEXCEPT.ipch
|-- |   |   |   |   |   |   |-- 8b5e606442870480/
|-- |   |   |   |   |   |   |   |-- NATIVESUBSYSTEM.ipch
|-- |   |   |   |   |   |   |-- 97c6b87ab909fa8/
|-- |   |   |   |   |   |   |   |-- REMOTEEXEC.ipch
|-- |   |   |   |   |   |   |-- b3d7bb5a9e7a7f/
|-- |   |   |   |   |   |   |   |-- ASMHELPER64.ipch
|-- |   |   |   |   |   |   |-- ef1cab2bc2b70ccc/
|-- |   |   |   |   |   |   |   |-- PROCESSMEMORY.ipch
|-- |   |   |   |   |   |   |-- f7dfa700f4f0b91/
|-- |   |   |   |   |   |   |   |-- DRIVERCONTROL.ipch
|-- |   |   |-- cmake.db
|-- |   |-- ALL_BUILD.vcxproj
|-- |   |-- ALL_BUILD.vcxproj.filters
|-- |   |-- ALL_BUILD.vcxproj.user
|-- |   |-- BlackBone/
|-- |   |   |-- src/
|-- |   |   |   |-- BlackBone/
|-- |   |   |   |   |-- ALL_BUILD.vcxproj
|-- |   |   |   |   |-- ALL_BUILD.vcxproj.filters
|-- |   |   |   |   |-- BlackBone.dir/
|-- |   |   |   |   |   |-- Debug/
|-- |   |   |   |   |   |   |-- AsmHelper32.obj
|-- |   |   |   |   |   |   |-- AsmHelper64.obj
|-- |   |   |   |   |   |   |-- BlackBone.lib.recipe
|-- |   |   |   |   |   |   |-- BlackBone.log
|-- |   |   |   |   |   |   |-- BlackBone.tlog/
|-- |   |   |   |   |   |   |   |-- BlackBone.lastbuildstate
|-- |   |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib-link.read.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib-link.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib.command.1.tlog
|-- |   |   |   |   |   |   |   |-- Masm.read.1u.tlog
|-- |   |   |   |   |   |   |   |-- Masm.write.1u.tlog
|-- |   |   |   |   |   |   |-- DriverControl.obj
|-- |   |   |   |   |   |   |-- ImageNET.obj
|-- |   |   |   |   |   |   |-- InitOnce.obj
|-- |   |   |   |   |   |   |-- LDasm.obj
|-- |   |   |   |   |   |   |-- LocalHookBase.obj
|-- |   |   |   |   |   |   |-- MExcept.obj
|-- |   |   |   |   |   |   |-- MMap.obj
|-- |   |   |   |   |   |   |-- MemBlock.obj
|-- |   |   |   |   |   |   |-- NameResolve.obj
|-- |   |   |   |   |   |   |-- NativeSubsystem.obj
|-- |   |   |   |   |   |   |-- NtLoader.obj
|-- |   |   |   |   |   |   |-- PDBHelper.obj
|-- |   |   |   |   |   |   |-- PEImage.obj
|-- |   |   |   |   |   |   |-- PatternLoader.obj
|-- |   |   |   |   |   |   |-- PatternSearch.obj
|-- |   |   |   |   |   |   |-- Process.obj
|-- |   |   |   |   |   |   |-- ProcessCore.obj
|-- |   |   |   |   |   |   |-- ProcessMemory.obj
|-- |   |   |   |   |   |   |-- ProcessModules.obj
|-- |   |   |   |   |   |   |-- RemoteExec.obj
|-- |   |   |   |   |   |   |-- RemoteHook.obj
|-- |   |   |   |   |   |   |-- RemoteLocalHook.obj
|-- |   |   |   |   |   |   |-- RemoteMemory.obj
|-- |   |   |   |   |   |   |-- SymbolData.obj
|-- |   |   |   |   |   |   |-- SymbolLoader.obj
|-- |   |   |   |   |   |   |-- Syscall64.obj
|-- |   |   |   |   |   |   |-- Thread.obj
|-- |   |   |   |   |   |   |-- Threads.obj
|-- |   |   |   |   |   |   |-- TraceHook.obj
|-- |   |   |   |   |   |   |-- Utils.obj
|-- |   |   |   |   |   |   |-- Wow64Subsystem.obj
|-- |   |   |   |   |   |   |-- assembler.obj
|-- |   |   |   |   |   |   |-- codegen.obj
|-- |   |   |   |   |   |   |-- constpool.obj
|-- |   |   |   |   |   |   |-- containers.obj
|-- |   |   |   |   |   |   |-- cpuinfo.obj
|-- |   |   |   |   |   |   |-- cputicks.obj
|-- |   |   |   |   |   |   |-- error.obj
|-- |   |   |   |   |   |   |-- globals.obj
|-- |   |   |   |   |   |   |-- operand.obj
|-- |   |   |   |   |   |   |-- runtime.obj
|-- |   |   |   |   |   |   |-- string.obj
|-- |   |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |   |   |-- vmem.obj
|-- |   |   |   |   |   |   |-- wow64ext.obj
|-- |   |   |   |   |   |   |-- x86Subsystem.obj
|-- |   |   |   |   |   |   |-- x86assembler.obj
|-- |   |   |   |   |   |   |-- x86cpuinfo.obj
|-- |   |   |   |   |   |   |-- x86inst.obj
|-- |   |   |   |   |   |   |-- x86operand.obj
|-- |   |   |   |   |   |   |-- x86operand_regs.obj
|-- |   |   |   |   |   |   |-- zone.obj
|-- |   |   |   |   |   |-- Release/
|-- |   |   |   |   |   |   |-- AsmHelper32.obj
|-- |   |   |   |   |   |   |-- AsmHelper64.obj
|-- |   |   |   |   |   |   |-- BlackBone.lib.recipe
|-- |   |   |   |   |   |   |-- BlackBone.log
|-- |   |   |   |   |   |   |-- BlackBone.tlog/
|-- |   |   |   |   |   |   |   |-- BlackBone.lastbuildstate
|-- |   |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib-link.read.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib-link.write.1.tlog
|-- |   |   |   |   |   |   |   |-- Lib.command.1.tlog
|-- |   |   |   |   |   |   |   |-- Masm.read.1u.tlog
|-- |   |   |   |   |   |   |   |-- Masm.write.1u.tlog
|-- |   |   |   |   |   |   |-- DriverControl.obj
|-- |   |   |   |   |   |   |-- ImageNET.obj
|-- |   |   |   |   |   |   |-- InitOnce.obj
|-- |   |   |   |   |   |   |-- LDasm.obj
|-- |   |   |   |   |   |   |-- LocalHookBase.obj
|-- |   |   |   |   |   |   |-- MExcept.obj
|-- |   |   |   |   |   |   |-- MMap.obj
|-- |   |   |   |   |   |   |-- MemBlock.obj
|-- |   |   |   |   |   |   |-- NameResolve.obj
|-- |   |   |   |   |   |   |-- NativeSubsystem.obj
|-- |   |   |   |   |   |   |-- NtLoader.obj
|-- |   |   |   |   |   |   |-- PDBHelper.obj
|-- |   |   |   |   |   |   |-- PEImage.obj
|-- |   |   |   |   |   |   |-- PatternLoader.obj
|-- |   |   |   |   |   |   |-- PatternSearch.obj
|-- |   |   |   |   |   |   |-- Process.obj
|-- |   |   |   |   |   |   |-- ProcessCore.obj
|-- |   |   |   |   |   |   |-- ProcessMemory.obj
|-- |   |   |   |   |   |   |-- ProcessModules.obj
|-- |   |   |   |   |   |   |-- RemoteExec.obj
|-- |   |   |   |   |   |   |-- RemoteHook.obj
|-- |   |   |   |   |   |   |-- RemoteLocalHook.obj
|-- |   |   |   |   |   |   |-- RemoteMemory.obj
|-- |   |   |   |   |   |   |-- SymbolData.obj
|-- |   |   |   |   |   |   |-- SymbolLoader.obj
|-- |   |   |   |   |   |   |-- Syscall64.obj
|-- |   |   |   |   |   |   |-- Thread.obj
|-- |   |   |   |   |   |   |-- Threads.obj
|-- |   |   |   |   |   |   |-- TraceHook.obj
|-- |   |   |   |   |   |   |-- Utils.obj
|-- |   |   |   |   |   |   |-- Wow64Subsystem.obj
|-- |   |   |   |   |   |   |-- assembler.obj
|-- |   |   |   |   |   |   |-- codegen.obj
|-- |   |   |   |   |   |   |-- constpool.obj
|-- |   |   |   |   |   |   |-- containers.obj
|-- |   |   |   |   |   |   |-- cpuinfo.obj
|-- |   |   |   |   |   |   |-- cputicks.obj
|-- |   |   |   |   |   |   |-- error.obj
|-- |   |   |   |   |   |   |-- globals.obj
|-- |   |   |   |   |   |   |-- operand.obj
|-- |   |   |   |   |   |   |-- runtime.obj
|-- |   |   |   |   |   |   |-- string.obj
|-- |   |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |   |   |-- vmem.obj
|-- |   |   |   |   |   |   |-- wow64ext.obj
|-- |   |   |   |   |   |   |-- x86Subsystem.obj
|-- |   |   |   |   |   |   |-- x86assembler.obj
|-- |   |   |   |   |   |   |-- x86cpuinfo.obj
|-- |   |   |   |   |   |   |-- x86inst.obj
|-- |   |   |   |   |   |   |-- x86operand.obj
|-- |   |   |   |   |   |   |-- x86operand_regs.obj
|-- |   |   |   |   |   |   |-- zone.obj
|-- |   |   |   |   |-- BlackBone.vcxproj
|-- |   |   |   |   |-- BlackBone.vcxproj.filters
|-- |   |   |   |   |-- BlackBoneLib.sln
|-- |   |   |   |   |-- CMakeFiles/
|-- |   |   |   |   |   |-- generate.stamp
|-- |   |   |   |   |   |-- generate.stamp.depend
|-- |   |   |   |   |-- Debug/
|-- |   |   |   |   |   |-- BlackBone.lib
|-- |   |   |   |   |   |-- BlackBone.pdb
|-- |   |   |   |   |-- Release/
|-- |   |   |   |   |   |-- BlackBone.lib
|-- |   |   |   |   |-- cmake_install.cmake
|-- |   |-- CMakeCache.txt
|-- |   |-- CMakeFiles/
|-- |   |   |-- 3.29.3/
|-- |   |   |   |-- CMakeASM_MASMCompiler.cmake
|-- |   |   |   |-- CMakeCCompiler.cmake
|-- |   |   |   |-- CMakeCXXCompiler.cmake
|-- |   |   |   |-- CMakeDetermineCompilerABI_C.bin
|-- |   |   |   |-- CMakeDetermineCompilerABI_CXX.bin
|-- |   |   |   |-- CMakeRCCompiler.cmake
|-- |   |   |   |-- CMakeSystem.cmake
|-- |   |   |   |-- CompilerIdASM_MASM/
|-- |   |   |   |-- CompilerIdC/
|-- |   |   |   |   |-- CMakeCCompilerId.c
|-- |   |   |   |   |-- CompilerIdC.exe
|-- |   |   |   |   |-- CompilerIdC.vcxproj
|-- |   |   |   |   |-- Debug/
|-- |   |   |   |   |   |-- CMakeCCompilerId.obj
|-- |   |   |   |   |   |-- CompilerIdC.exe.recipe
|-- |   |   |   |   |   |-- CompilerIdC.tlog/
|-- |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |-- CompilerIdC.lastbuildstate
|-- |   |   |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |-- tmp/
|-- |   |   |   |-- CompilerIdCXX/
|-- |   |   |   |   |-- CMakeCXXCompilerId.cpp
|-- |   |   |   |   |-- CompilerIdCXX.exe
|-- |   |   |   |   |-- CompilerIdCXX.vcxproj
|-- |   |   |   |   |-- Debug/
|-- |   |   |   |   |   |-- CMakeCXXCompilerId.obj
|-- |   |   |   |   |   |-- CompilerIdCXX.exe.recipe
|-- |   |   |   |   |   |-- CompilerIdCXX.tlog/
|-- |   |   |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |   |   |-- CompilerIdCXX.lastbuildstate
|-- |   |   |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |   |-- tmp/
|-- |   |   |   |-- VCTargetsPath/
|-- |   |   |   |   |-- x64/
|-- |   |   |   |   |   |-- Debug/
|-- |   |   |   |   |   |   |-- VCTargetsPath.recipe
|-- |   |   |   |   |   |   |-- VCTargetsPath.tlog/
|-- |   |   |   |   |   |   |   |-- VCTargetsPath.lastbuildstate
|-- |   |   |   |-- VCTargetsPath.txt
|-- |   |   |   |-- VCTargetsPath.vcxproj
|-- |   |   |   |-- x64/
|-- |   |   |   |   |-- Debug/
|-- |   |   |-- CMakeConfigureLog.yaml
|-- |   |   |-- TargetDirectories.txt
|-- |   |   |-- cbcae766f36b7b5b68c018ccc9b56478/
|-- |   |   |   |-- generate.stamp.rule
|-- |   |   |-- cmake.check_cache
|-- |   |   |-- generate.stamp
|-- |   |   |-- generate.stamp.depend
|-- |   |   |-- generate.stamp.list
|-- |   |   |-- pkgRedirects/
|-- |   |-- Debug/
|-- |   |   |-- AppCore.dll
|-- |   |   |-- MoonInjector.exe
|-- |   |   |-- MoonInjector.exp
|-- |   |   |-- MoonInjector.lib
|-- |   |   |-- MoonInjector.pdb
|-- |   |   |-- Ultralight.dll
|-- |   |   |-- UltralightCore.dll
|-- |   |   |-- WebCore.dll
|-- |   |   |-- resources/
|-- |   |   |   |-- cacert.pem
|-- |   |   |   |-- icudt67l.dat
|-- |   |-- MoonInjector.dir/
|-- |   |   |-- Debug/
|-- |   |   |   |-- MoonInjector.exe.recipe
|-- |   |   |   |-- MoonInjector.ilk
|-- |   |   |   |-- MoonInjector.log
|-- |   |   |   |-- MoonInjector.tlog/
|-- |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |-- MoonInjector.lastbuildstate
|-- |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |-- dx_setup.obj
|-- |   |   |   |-- imgui.obj
|-- |   |   |   |-- imgui_draw.obj
|-- |   |   |   |-- imgui_impl_dx11.obj
|-- |   |   |   |-- imgui_impl_win32.obj
|-- |   |   |   |-- imgui_tables.obj
|-- |   |   |   |-- imgui_widgets.obj
|-- |   |   |   |-- injector.obj
|-- |   |   |   |-- main.obj
|-- |   |   |   |-- menu.obj
|-- |   |   |   |-- tinyfiledialogs.obj
|-- |   |   |   |-- ui_style.obj
|-- |   |   |   |-- ultralight_controller.obj
|-- |   |   |   |-- vc143.pdb
|-- |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |-- window_setup.obj
|-- |   |   |-- Release/
|-- |   |   |   |-- MoonInjector.exe.recipe
|-- |   |   |   |-- MoonInjector.log
|-- |   |   |   |-- MoonInjector.tlog/
|-- |   |   |   |   |-- CL.command.1.tlog
|-- |   |   |   |   |-- CL.read.1.tlog
|-- |   |   |   |   |-- CL.write.1.tlog
|-- |   |   |   |   |-- Cl.items.tlog
|-- |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |-- MoonInjector.lastbuildstate
|-- |   |   |   |   |-- link.command.1.tlog
|-- |   |   |   |   |-- link.read.1.tlog
|-- |   |   |   |   |-- link.secondary.1.tlog
|-- |   |   |   |   |-- link.write.1.tlog
|-- |   |   |   |-- dx_setup.obj
|-- |   |   |   |-- imgui.obj
|-- |   |   |   |-- imgui_draw.obj
|-- |   |   |   |-- imgui_impl_dx11.obj
|-- |   |   |   |-- imgui_impl_win32.obj
|-- |   |   |   |-- imgui_tables.obj
|-- |   |   |   |-- imgui_widgets.obj
|-- |   |   |   |-- injector.obj
|-- |   |   |   |-- main.obj
|-- |   |   |   |-- menu.obj
|-- |   |   |   |-- tinyfiledialogs.obj
|-- |   |   |   |-- ui_style.obj
|-- |   |   |   |-- ultralight_controller.obj
|-- |   |   |   |-- vcpkg.applocal.log
|-- |   |   |   |-- window_setup.obj
|-- |   |-- MoonInjector.sln
|-- |   |-- MoonInjector.vcxproj
|-- |   |-- MoonInjector.vcxproj.filters
|-- |   |-- MoonInjector.vcxproj.user
|-- |   |-- Release/
|-- |   |   |-- AppCore.dll
|-- |   |   |-- BlackBoneDrv10.sys
|-- |   |   |-- MoonInjector.exe
|-- |   |   |-- MoonInjector.exp
|-- |   |   |-- MoonInjector.lib
|-- |   |   |-- Ultralight.dll
|-- |   |   |-- UltralightCore.dll
|-- |   |   |-- WebCore.dll
|-- |   |   |-- default/
|-- |   |   |   |-- cookie.jar.db
|-- |   |   |   |-- cookie.jar.db-shm
|-- |   |   |   |-- cookie.jar.db-wal
|-- |   |   |-- resources/
|-- |   |   |   |-- cacert.pem
|-- |   |   |   |-- icudt67l.dat
|-- |   |   |-- tutorial.txt
|-- |   |-- ZERO_CHECK.vcxproj
|-- |   |-- ZERO_CHECK.vcxproj.filters
|-- |   |-- cmake_install.cmake
|-- |   |-- default/
|-- |   |   |-- cookie.jar.db
|-- |   |   |-- cookie.jar.db-shm
|-- |   |   |-- cookie.jar.db-wal
|-- |   |-- x64/
|-- |   |   |-- Debug/
|-- |   |   |   |-- ZERO_CHECK/
|-- |   |   |   |   |-- ZERO_CHECK.log
|-- |   |   |   |   |-- ZERO_CHECK.recipe
|-- |   |   |   |   |-- ZERO_CHECK.tlog/
|-- |   |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |   |-- ZERO_CHECK.lastbuildstate
|-- |   |   |-- Release/
|-- |   |   |   |-- ZERO_CHECK/
|-- |   |   |   |   |-- ZERO_CHECK.log
|-- |   |   |   |   |-- ZERO_CHECK.recipe
|-- |   |   |   |   |-- ZERO_CHECK.tlog/
|-- |   |   |   |   |   |-- CustomBuild.command.1.tlog
|-- |   |   |   |   |   |-- CustomBuild.read.1.tlog
|-- |   |   |   |   |   |-- CustomBuild.write.1.tlog
|-- |   |   |   |   |   |-- ZERO_CHECK.lastbuildstate
|-- |-- dx_setup.cpp
|-- |-- dx_setup.h
|-- |-- font.h
|-- |-- icons.h
|-- |-- injector.cpp
|-- |-- injector.h
|-- |-- main.cpp
|-- |-- menu.cpp
|-- |-- menu.h
|-- |-- project.code-workspace
|-- |-- ui_style.cpp
|-- |-- ui_style.h
|-- |-- ultralight_controller.cpp
|-- |-- ultralight_controller.h
|-- |-- window_setup.cpp
|-- |-- window_setup.h

