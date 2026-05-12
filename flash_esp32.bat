@echo off
set "MSYSTEM="
set "MSYS="
set "IDF_PATH=C:\esp\v6.0.1\esp-idf"
set "IDF_TOOLS_PATH=C:\Espressif\tools"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v6.0.1\venv"
set "ESP_IDF_VERSION=6.0.1"
set "PATH=C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\python\v6.0.1\venv\Scripts;%PATH%"
cd /d C:\Users\roarp\Desktop\TMP\Code\AICode\ESP32\ESP32RLCD42
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe C:\esp\v6.0.1\esp-idf\tools\idf.py -p COM3 flash
