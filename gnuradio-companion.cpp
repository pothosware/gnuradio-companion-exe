// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Windows.h>
#include <Winreg.h>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <ciso646>
#include <vector>

/***********************************************************************
 * Extract the python 2.7 install path from the registry
 **********************************************************************/
static std::string getPython27ExePath(void)
{
    HKEY key;

    const std::string regPath("SOFTWARE\\Python\\PythonCore\\2.7\\InstallPath");
    LONG ret = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        regPath.c_str(), 0, KEY_READ, &key);

    if (ret != ERROR_SUCCESS) throw std::runtime_error(
        "Failed to open registry key HKLM\\" + regPath +
        "\nIs Python 2.7 installed?");

    char pathStr[512];
    DWORD pathSize = sizeof(pathStr);
    ret |= RegQueryValueEx(
        key, "", nullptr, nullptr,
        (LPBYTE)&pathStr, &pathSize);
    RegCloseKey(key);

    if (ret != ERROR_SUCCESS) throw std::runtime_error(
        "Failed to read registry key HKLM\\" + regPath +
        "\nPossible Python 2.7 install issue.");

    const std::string pythonPath = std::string(pathStr) + "\\python.exe";
    DWORD fileAtt = GetFileAttributesA(pythonPath.c_str());
    if (fileAtt == INVALID_FILE_ATTRIBUTES) throw std::runtime_error(pythonPath + " does not exist!");

    return pythonPath;
}

/***********************************************************************
 * Extract executable path to locate scripts
 **********************************************************************/
static std::string getGnuradioCompanionPath(void)
{
    char path[MAX_PATH];
    HMODULE hm = NULL;
    if (not GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &getGnuradioCompanionPath, &hm)
    )
        throw std::runtime_error("Failed to get handle to this executable!");

    const DWORD size = GetModuleFileNameA(hm, path, sizeof(path));
    if (size == 0) throw std::runtime_error("Failed to get file name of this executable!");

    const std::string exePath(path, size);
    const size_t slashPos = exePath.find_last_of("/\\");
    if (slashPos == std::string::npos) throw std::runtime_error(
        "Failed to parse directory path of this executable!"
        "\n'"+exePath+"'");

    const auto grcPath = exePath.substr(0, slashPos) + "\\gnuradio-companion.py";
    DWORD fileAtt = GetFileAttributesA(grcPath.c_str());
    if (fileAtt == INVALID_FILE_ATTRIBUTES) throw std::runtime_error(grcPath + " does not exist!"
        "\nPossible installation issue.");

    return grcPath;
}

/***********************************************************************
 * Helper to execute a process and wait for completion
 **********************************************************************/
const int execProcess(const std::vector<std::string> &args, const DWORD flags)
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
        pythonExe = getPython27ExePath();
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Python 2.7 inspection failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //extract gnuradio companion path
    std::string grcPy;
    try
    {
        grcPy = getGnuradioCompanionPath();
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Gnuradio Companion location failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    //execute gnuradio companion with args
    try
    {
        std::vector<std::string> args;
        args.push_back(pythonExe);
        args.push_back(grcPy);
        for (size_t i = 1; i < argc; i++) args.push_back(argv[i]);
        return execProcess(args, CREATE_NO_WINDOW);
    }
    catch (const std::exception &ex)
    {
        MessageBox(nullptr, ex.what(), "Gnuradio Companion exec failed!", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
