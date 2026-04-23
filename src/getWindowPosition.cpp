//
// Created by AyanMR on 26-2-24.
//

#include "getWindowPosition.h"

#include <cwchar>
#include <optional>
#include <string>

namespace
{
    const wchar_t *GetBaseName(const wchar_t *path)
    {
        if (!path)
        {
            return L"";
        }
        const wchar_t *lastSlash = wcsrchr(path, L'\\');
        return lastSlash ?
                   lastSlash + 1 :
                   path;
    }

    bool IsNameMatch(const wchar_t *baseName, const wchar_t *procName)
    {
        if (!baseName || !procName)
        {
            return false;
        }
        if (_wcsicmp(baseName, procName) == 0)
        {
            return true;
        }
        std::wstring withExe(procName);
        if (withExe.find(L'.') == std::wstring::npos)
        {
            withExe += L".exe";
        }
        return _wcsicmp(baseName, withExe.c_str()) == 0;
    }

    struct FindContext
    {
        const wchar_t *procName;
        RECT *         outRect;
        bool           found;
    };

    BOOL CALLBACK EnumWindowsByProcessProc(HWND hwnd, LPARAM lParam)
    {
        auto *context = reinterpret_cast < FindContext * >(lParam);
        if (!context || !context->procName || !context->outRect)
        {
            return TRUE;
        }

        if (!IsWindowVisible(hwnd))
        {
            return TRUE;
        }

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId == 0)
        {
            return TRUE;
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!process)
        {
            return TRUE;
        }

        wchar_t path[MAX_PATH] = {0};
        DWORD   size           = std::size(path);
        bool    matched        = false;
        if (QueryFullProcessImageNameW(process, 0, path, &size))
        {
            const wchar_t *baseName = GetBaseName(path);
            matched                 = IsNameMatch(baseName, context->procName);
        }
        CloseHandle(process);

        if (!matched)
        {
            return TRUE;
        }

        RECT rect{};
        if (!GetWindowRect(hwnd, &rect))
        {
            return TRUE;
        }

        *context->outRect = rect;
        context->found    = true;
        return FALSE;
    }
} // namespace

std::optional < RECT > GetWindowRectByProcessName(const wchar_t *procName)
{
    if (!procName || !*procName)
    {
        return std::nullopt;
    }
    RECT        outRect{};
    FindContext context{procName, &outRect, false};
    EnumWindows(EnumWindowsByProcessProc, reinterpret_cast < LPARAM >(&context));
    if (context.found)
    {
        return outRect;
    }
    return std::nullopt;
}
