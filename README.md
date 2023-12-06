# Smart Sole for Diabetic Foot Disease

GitHub repository for the application of a smart sole for diabetic foot disease of an ESP32-C3. 

It is intended to be used with custom developed hardware (insole with sensors) and a smartphone app. This repo only provides open access to the microcontrollers code.

# Setup

## Code Editor

For developing the ESP32 the code editor Visual Studio Code (VS Code) is recommended. Here the extension for the Espressif IDF is used.

- First install VS Code from Microsoft found [here](https://code.visualstudio.com/docs/?dv=win)
- When installed, start VS Code and click on "Extensions" (Four small squares, alternatively press Ctrl+Shift+X)
- Here install the extension named "Espressif IDF"
- After installing the extension VS Code needs to be restarted

### Automatic ESP-IDF Installation**

- After restarting a new symbol should be visible on the left bar, the "ESP-IDF Explorer". Click on the symbol to go to the extensions menu
- In VS Code the "ESP-IDF Setup" window should open. This is used to install the ESP-IDF directly in VS Code. If the installation should be installed manually, follow the instructions in [Manual Installation](#manual-installation)

> **If the user path is empty or contains special characters follow the installation [here](#sonder-installation)**

- In the setup window choose the express option. This will install the ESP-IDF in the user folder (recommended)
- **When choosing the version select v5.1.x** , if using another version, especially older versions, the code may not be compatible with the IDF and may not compile correctly or at all
- After this click install and wait until the installation is finished

### **Sonder-Installation**

In the case the user path contains spaces or special characters.

- In the setup window choose "Advanced Installation". Here the installation path can be customized

- Create a folder on your computer where the ESP-IDF should be install in a location not containing spaces or special characters

  > Example for Windows: `C:\ESP-IDF`

- In this folder create 2 subfolders named `esp-tools` and `esp-idf`

  > The `esp-idf` folder should be created automatically by the installer but can be created manually to make sure

- In the advanced install menu now choose the folders for the IDF and the tools and choose version `v5.1.x`

- After this click install and wait until the installation is finished

### **Manual Installation**

When the ESP-IDF needs to be installed manually, follow the [instruction for installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html) by Espressif.

**Here also choose version v5.1.x**

After this choose "Advanced Installation" in the ESP-IDF installation in VS Code and choose the path of the manually installed IDF installation.

### **After the Installation**

For VS Code to find the build scripts for the IDF, following paths need to be added to the PATH environment variable (Windows example):

- `C:\Users\*Username*\.espressif\tools\idf-python\3.11.2`

  > ESP-IDF Python Environment

- `C:\Users\*Username*\esp\esp-idf\tools`

  > ESP-IDF Tools (Python scripts of the IDF)

>When using a different installation path the path added to the environment variable needs to be changed accordingly!

After the path is added, restart VS Code.

- In VS Code the project folder (this git repo) can be opened
- Now the ESP-IDF can be used with the buttons in the lowermost bar of the VS Code UI
- This includes button to build, flash and monitor the chip

#### **Code Completion and adding ESP location**

For VS Code to also analyze the source code of the IDF, the path needs to be added into the C included of VS Code.

- In VS Code click on `View` on the top bar and choose `Command Palette…`
- Here type the command `C/C++: Edit configurations (UI)` and click on it
- In the newly opened window search for the config `Include Path` and add a new line with the content `${config:idf.espIdfPathWin}/components/**`
- The C code analyzer should now use the IDF source code and provide code completion

### **Others & Tips**

- Before flashing the serial port must be set in the lowermost bar at the left
- When using the IDF monitor in VS Code for displaying serial communication, the key combinations `Ctrl+T & Ctrl+X` need to be pressed after each other and in this order to close the monitor. The standard layout is `Ctrl+]` and is intended for US keyboard layout
