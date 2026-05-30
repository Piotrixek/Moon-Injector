# Moon Injector

![image](./mooninjectorimage.png)

Moon Injector is a modern, feature-rich DLL injection utility for Windows. It combines a highly responsive user interface built using [Ultralight](https://ultralig.ht/) (HTML5/CSS3/Tailwind) and [Dear ImGui](https://github.com/ocornut/imgui/tree/docking) (DirectX 11) with the injection capabilities of the [BlackBone](https://github.com/darthton/blackbone) library.

> ⚠️ **Important Note on Cloning/Downloading:** Due to file size restrictions, the `BlackBone` source folder is **not** included in this repository. To compile this project, you must download it directly from the official [BlackBone GitHub Repository](https://github.com/darthton/blackbone) and place it manually inside the project's root folder as `./BlackBone`.

The application utilizes an offline SQLite database via [SQLiteCpp](https://github.com/srombauts/sqlitecpp) to organize DLLs into customizable "Workspaces," allowing you to manage and deploy different injection targets with minimal effort.

---

## Features

* **Modern HTML UI Overlay**: Rendered via Ultralight and TailwindCSS inside a DirectX 11 ImGui frame.
* **Workspace Management**: Organizes your target DLLs.
* **Active Process Filtering**: Instantly search through active system processes, showing details such as Process ID (PID) and architecture (x86 vs x64).
* **Multiple Injection Methods**:
* **LoadLibrary (Standard)**: Standard Windows API injection. Does *not* require kernel drivers.
* **APC Injection**: Leverages Asynchronous Procedure Calls to execute code within target threads.
* **Thread Hijacking**: Suspends a target thread to redirect execution to load the DLL.
* **Manual Map (BlackBone)**: Manually maps the DLL into memory, bypassing standard API hooks. Includes optional features like **Erase PE Headers** and **Hide VAD**.
* **Driver Stealth (BlackBone)**: Uses kernel driver features for highly obfuscated mapping.


* **Auto-Inject**: Automatically attempts injection as soon as a target process is selected.
* **In-App Logs**: Live logging panel detailing the success or failure of each step during the injection cycle.

---

## Injection Methods & Driver Requirements

Moon Injector utilizes the [BlackBone](https://github.com/darthton/blackbone) library to offer various levels of stealth and access. Because of Windows security policies, these methods are divided into user-mode (driverless) and kernel-mode (driver required):

### 1. Driverless Methods (No Setup Required)

* **LoadLibrary**, **APC Injection**, and **Thread Hijacking** generally run using standard Windows user-mode APIs.
* These do not require any configuration changes to your operating system.

### 2. Kernel-Mode Methods (Driver Required)

* **BlackBone (Manual Map)** and **BlackBone (Driver Stealth)** require loading the BlackBone kernel driver (`BlackBoneDrv10.sys`).
* Because the driver compiled on your machine is unsigned, Windows will block it by default with the following error:
`0xC0000428 (STATUS_INVALID_IMAGE_HASH)`

To use these advanced methods, you must enable **Test Signing Mode** to allow unsigned developers' drivers to load.

---

## Setting Up Test Signing Mode (For Manual Map / Stealth)

To bypass the driver signature enforcement restriction during development and testing, follow these steps:

1. **Open Command Prompt as Administrator**:
* Click the Start Menu and type `cmd`.
* Right-click **Command Prompt** and select **Run as administrator**.


2. **Enable Test Signing**:
* In the command prompt window, input the following command and press Enter:
```cmd
bcdedit /set testsigning on

```


* You should see the message: `The operation completed successfully.`


3. **Restart Your Computer**:
* Reboot your PC to apply the changes.
* Once restarted, you will see a small "Test Mode" watermark in the bottom-right corner of your desktop. This indicates that Windows will now permit unsigned testing drivers to run.


4. **Verify**:
* Keep the `BlackBoneDrv10.sys` file in the same directory as `MoonInjector.exe`.
* Run Moon Injector as Administrator, select **BlackBone** or **Stealth** injection, and perform the injection.



> **To Disable Test Mode**: When you want to revert your system back to normal, open an administrator command prompt, run:
> ```cmd
> bcdedit /set testsigning off
> 
> ```
> 
> 
> and restart your computer.

---

## Project Structure & Dependencies

The project is configured using CMake. If you wish to build it from source, ensure you have the following SDKs placed in the correct directories (or update the paths in `CMakeLists.txt` accordingly):

### Third-Party Library:

* **Dear ImGui (docking branch)**
* **Ultralight SDK**
* **Tinyfiledialogs**
* **SQLiteCpp**
* **BlackBone**

### Paths defined in `CMakeLists.txt`:

* **Dear ImGui**: `C:/libs/imgui-docking`
* **Ultralight SDK**: `C:/libs/Ultralight`
* **Tinyfiledialogs**: `C:/libs/tinyfiledialogs`
* **SQLiteCpp**: `C:/libs/SQLiteCpp-3.3.3`
* **BlackBone**: Local project workspace under `./BlackBone` *(Reminder: You must download this folder manually from GitHub and drop it here)*

### Build Requirements

* **Visual Studio 2019 / 2022** (with C++ Desktop Development payload)
* **Windows SDK 10 / 11**
* **CMake 3.15+**

---

## Usage Guide

1. **Launch the Injector**: Ensure you run `MoonInjector.exe` as an **Administrator** (required to access process memory for most injection types).
2. **Workspaces**:
* Click the **+** button in the workspaces column to add a new workspace.
* Highlight a workspace to view its associated DLLs.


3. **Select target DLLs**:
* Click **Add** under the injection list and select the target `.dll` files using the file dialog.
* Click **Save Changes** to commit them to the local database.


4. **Choose Target Process**:
* Use the process filter input to locate your target application.
* Select it from the list.


5. **Select Method and Inject**:
* Choose your preferred injection method from the dropdown menu.
* Click **INJECT** to begin the procedure. Observe the status logs for real-time diagnostics.



---

## Disclaimer

This software is designed solely for educational purposes, software development debugging, and authorized security analysis. Using this tool to inject code into software without explicit permission is a violation of most terms of service and can be detected by modern anti-cheat systems. Use responsibly.
