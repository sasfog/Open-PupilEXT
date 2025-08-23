#pragma once

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_UNIX // any linux or Apple
#include <QProcess>
#include <unistd.h>
#endif

class AdminPrivileges {

public:

#ifdef Q_OS_WIN
    static bool isRunningAsAdmin() {
        BOOL isAdmin = false;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin;
    }

    static bool restartAsAdmin(const QStringList &argsL) {

        QString appPath = QCoreApplication::applicationFilePath();
        QString params = argsL.join(' ');

        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.lpVerb = L"runas";  // <- tells Windows to prompt for elevation
        sei.lpFile = (LPCWSTR) appPath.utf16();
        sei.lpParameters = (LPCWSTR) params.utf16();
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&sei)) {
            return false; // User probably clicked "No" in UAC
        }
        return true;
    }

#endif

#ifdef Q_OS_UNIX
    static bool isRunningAsAdmin() {
        return (geteuid() == 0);
    }

#ifdef Q_OS_LINUX \
// ANY LINUX
    static bool restartAsAdmin(const QStringList &argsL) {

        QString appPath = QCoreApplication::applicationFilePath();
        QStringList params;
        params << appPath;
        params << argsL.join(' ');

        // pkexec will trigger GUI password prompt if needed
        bool started = QProcess::startDetached("pkexec", params);
        return started;
    }
#else // #ifdef Q_OS_MACOS
    // APPLE
    static bool relaunchAsAdminMac(const QStringList &argsL)
    {
        QString appPath = QCoreApplication::applicationFilePath();
        QString script = QString(
            "do shell script \"%1 %2\" with administrator privileges")
            .arg(appPath)
            .arg(argsL.join(' '););

        QStringList osaArgs;
        osaArgs << "-e" << script;

        bool started = QProcess::startDetached("osascript", osaArgs);
        return started;
    }
#endif

#endif

};
