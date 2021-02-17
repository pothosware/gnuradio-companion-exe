// Copyright (c) 2016-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Windows.h>
#include <Winreg.h>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <ciso646>
#include <vector>
#include <cctype>

/***********************************************************************
 * True if a file path exists
 **********************************************************************/
const bool fileExists(const std::string &path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

/***********************************************************************
 * Insert into the environment specified by name
 **********************************************************************/
const void insertEnvPath(const char *name, const std::string &value)
{
    char originalPath[(32*1024)-1];
    const DWORD numRead = GetEnvironmentVariable(name, originalPath, sizeof(originalPath));
    std::string newPath(value);
    if (numRead > 0)
    {
        newPath += ";";
        newPath += originalPath;
    }
    if (not SetEnvironmentVariable(name, newPath.c_str())) throw std::runtime_error(
        "Failed to insert " + value + " into the " + std::string(name) + " environment variable");
}

/***********************************************************************
 * Only keep digits in a string
 **********************************************************************/
static std::string keepDigits(const std::string &s)
{
    std::string out;
    for (const auto &c : s) if (std::isdigit(c)) out += c;
    return out;
}

/***********************************************************************
 * Common paths for the python executable
 **********************************************************************/
static std::string getPythonExePathLocalUser(void)
{
    const std::string suffix("\\Programs\\Python\\Python" + keepDigits(PYTHON_VERSION) + "\\python.exe");
    char pathStr[512];
    DWORD ret = GetEnvironmentVariable("LOCALAPPDATA", pathStr, sizeof(pathStr));
    if (ret <= 0) return "";
    return std::string(pathStr, ret) + suffix;
}

static std::string getPythonExePathGlobalUser(void)
{
    const std::string suffix("\\Python" + keepDigits(PYTHON_VERSION) + "\\python.exe");
    char pathStr[512];
    DWORD ret = GetEnvironmentVariable("PROGRAMFILES", pathStr, sizeof(pathStr));
    if (ret <= 0) return "";
    return std::string(pathStr, ret) + suffix;
}

static const std::string regPath("SOFTWARE\\Python\\PythonCore\\" + std::string(PYTHON_VERSION) + "\\InstallPath");

static std::string getPythonExePathRegistry(void)
{
    HKEY key;
    LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &key);
    if (ret != ERROR_SUCCESS) return "";

    char pathStr[512];
    DWORD pathSize = sizeof(pathStr);
    ret |= RegQueryValueEx(key, "", nullptr, nullptr, (LPBYTE)&pathStr, &pathSize);
    RegCloseKey(key);
    if (ret != ERROR_SUCCESS) return "";

    return std::string(pathStr) + "\\python.exe";
}

/***********************************************************************
 * Find a python executable on the system
 **********************************************************************/
static std::string getPythonExePath(void)
{
    std::vector<std::string> paths;

    std::string errorMsg("Failed to find amd64 python.exe:\n");
    for (const auto &path : {
        getPythonExePathLocalUser(),
        getPythonExePathGlobalUser(),
        getPythonExePathRegistry()})
    {
        if (path.empty()) continue;
        DWORD binaryType;
        const bool binTypeOk = GetBinaryTypeA(path.c_str(), &binaryType);
        const bool is64Bit = binTypeOk and binaryType == SCS_64BIT_BINARY;
        errorMsg += path;
        if (not fileExists(path)) errorMsg += " (not found)";
        else if (not is64Bit) errorMsg += " (not amd64)";
        else paths.push_back(path);
        errorMsg += "\n";
    }

    //list the HKEY_LOCAL_MACHINE search path as well
    errorMsg += "[HKLM]" + regPath + "\n";

    //found a result, return the first one
    if (not paths.empty()) return paths.front();

    //otherwise throw the error message
    throw std::runtime_error(errorMsg);
}

/***********************************************************************
 * Extract executable path to locate scripts
 **********************************************************************/
static std::string getExeDirectoryPath(void)
{
    char path[MAX_PATH];
    HMODULE hm = NULL;
    if (not GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &getExeDirectoryPath, &hm)
    )
        throw std::runtime_error("Failed to get handle to this executable!");

    const DWORD size = GetModuleFileNameA(hm, path, sizeof(path));
    if (size == 0) throw std::runtime_error("Failed to get file name of this executable!");

    const std::string exePath(path, size);
    const size_t slashPos = exePath.find_last_of("/\\");
    if (slashPos == std::string::npos) throw std::runtime_error(
        "Failed to parse directory path of this executable!");
    return exePath.substr(0, slashPos);
}

static std::string getRootDirectoryPath(void)
{
    const auto exePath = getExeDirectoryPath();
    const size_t slashPos = exePath.find_last_of("/\\");
    if (slashPos == std::string::npos) throw std::runtime_error(
        "Failed to traverse up a directory from the exe path!");
    return exePath.substr(0, slashPos);
}

/***********************************************************************
 * Helper to execute a process and wait for completion
 **********************************************************************/
const int execProcess(const std::vector<std::string> &args, const DWORD flags = 0)
{
    std::string command;
    for (const auto &arg : args)
    {
        if (not command.empty()) command += " ";
        command += "\"" + arg + "\"";
    }

    char cmdIo[MAX_PATH];
    std::memset(cmdIo, 0, sizeof(cmdIo));
    std::memcpy(cmdIo, command.c_str(), command.size());

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, cmdIo, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi) == 0)
    {
        throw std::runtime_error("Failed to execute: " + command);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

/***********************************************************************
 * Run all relevant checks and execute gnuradio-companion.py
 **********************************************************************/
int main(int argc, char **argv)
{
    //extract python executable path
    std::string pythonExe;
    try
    {
        pythonExe = getPythonExePath();
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Python exe inspection failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //extract gnuradio companion path
    std::string grcPath;
    try
    {
        grcPath = getExeDirectoryPath() + "\\gnuradio-companion.py";
        if (not fileExists(grcPath)) throw std::runtime_error(grcPath + " does not exist!"
            "\nPossible installation issue.");
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Gnuradio Companion location failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //setup the environment
    try
    {
        //set the python path in case that the installer did not register the modules
        insertEnvPath("PYTHONPATH", getRootDirectoryPath() + "\\lib\\python"+PYTHON_VERSION+"\\site-packages");

        //point GRC to its blocks in case that its not set by the installer
        insertEnvPath("GRC_BLOCKS_PATH", getRootDirectoryPath() + "\\share\\gnuradio\\grc\\blocks");

        //installer runtime DLLs (top priority)
        insertEnvPath("PATH", getExeDirectoryPath());
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Environment configuration failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //execute gnuradio companion with args
    int exitCode = EXIT_SUCCESS;
    try
    {
        std::vector<std::string> args;
        args.push_back(pythonExe);
        args.push_back(grcPath);
        for (size_t i = 1; i < argc; i++) args.push_back(argv[i]);
        exitCode = execProcess(args, CREATE_NO_WINDOW);
        if (exitCode == 0) return 0;
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Gnuradio Companion exec failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //on failure, execute the gnuradio helper
    int ret = MessageBox(nullptr,
        "Would you like to run GNURadioHelper.py to inspect the installation and attempt to fix the problem?",
        "Gnuradio Companion exited with error!", MB_YESNO | MB_ICONQUESTION);

    if (ret == IDYES) try
    {
        const std::string gnuradioHelper = getExeDirectoryPath() + "\\GNURadioHelper.py";
        if (not fileExists(gnuradioHelper)) throw std::runtime_error("Gnuradio Helper script missing: " + gnuradioHelper);
        std::vector<std::string> args;
        args.push_back(pythonExe);
        args.push_back(gnuradioHelper);
        return execProcess(args);
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Gnuradio Helper script failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return exitCode;
}
