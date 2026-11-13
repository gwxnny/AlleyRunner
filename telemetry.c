#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <process.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "telemetry.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

/* Paste only /macros/s/YOUR_DEPLOYMENT_ID/exec here, not the full URL. */
static const wchar_t endpoint[] = L"/macros/s/AKfycbycrVC9ZXpG-VuEqb2H6vcJ00OGZmduzicYOMUpR3W4Nw60-pb6RJIKFrpgMmfN2_0/exec";
static volatile LONG activeJobs = 0;
static volatile LONG uploaded = 0;
static volatile LONG failed = 0;

typedef struct UploadJob {
    char json[512];
} UploadJob;

static bool PostResult(const char* json)
{
    HINTERNET session = NULL, connection = NULL, request = NULL;
    bool ok = false;
    DWORD status = 0, statusBytes = sizeof(status), used = 0;
    char response[2048];
    char compact[2048];
    size_t n = 0;

    session = WinHttpOpen(L"AlleyRunnerPlaytest/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;
    if (!WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000)) goto cleanup;

    connection = WinHttpConnect(session, L"script.google.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) goto cleanup;
    request = WinHttpOpenRequest(connection, L"POST", endpoint,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!request) goto cleanup;

    /* Leave normal redirects enabled to retrieve Google's response. */
    if (!WinHttpSendRequest(request,
        L"Content-Type: application/json\r\n", (DWORD)-1L,
        (LPVOID)json, (DWORD)strlen(json), (DWORD)strlen(json), 0)) goto cleanup;
    if (!WinHttpReceiveResponse(request, NULL)) goto cleanup;
    if (!WinHttpQueryHeaders(request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusBytes,
        WINHTTP_NO_HEADER_INDEX)) goto cleanup;
    if (status != 200) goto cleanup;

    for (;;) {
        DWORD got = 0;
        if (used >= sizeof(response) - 1) goto cleanup;
        if (!WinHttpReadData(request, response + used,
            (DWORD)(sizeof(response) - 1 - used), &got)) goto cleanup;
        if (!got) break;
        used += got;
    }
    response[used] = '\0';

    /* Our receiver returns a tiny JSON object; reject HTML/login pages. */
    for (DWORD i = 0; i < used; ++i) {
        char ch = response[i];
        if (ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t')
            compact[n++] = ch;
    }
    compact[n] = '\0';
    ok = strcmp(compact, "{\"ok\":true}") == 0 ||
        strcmp(compact, "{\"ok\":true,\"duplicate\":true}") == 0;

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static unsigned __stdcall UploadWorker(void* argument)
{
    UploadJob* job = (UploadJob*)argument;
    bool ok = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt) Sleep(2000);
        /* Reuse the same JSON and Run ID for every retry. */
        if (PostResult(job->json)) {
            ok = true;
            break;
        }
    }
    if (ok) InterlockedIncrement(&uploaded);
    else InterlockedIncrement(&failed);
    free(job);
    InterlockedDecrement(&activeJobs);
    return 0;
}

bool Telemetry_SubmitRun(int score, float seconds, int coins, const char* cause)
{
    unsigned char randomBytes[16];
    char runId[33];
    UploadJob* job;
    uintptr_t thread;
    int length;

    if (!cause || score < 0 || score > 100000000 ||
        !isfinite(seconds) || seconds < 0 || seconds > 86400 ||
        coins < 0 || coins > 1000000) return false;
    /* Only known strings are inserted into JSON. */
    if (strcmp(cause, "ball") && strcmp(cause, "barrier") &&
        strcmp(cause, "oil") && strcmp(cause, "hole") &&
        strcmp(cause, "unknown")) return false;
    if (wcsstr(endpoint, L"REPLACE_WITH")) return false;

    /* Bound the number of simultaneous background uploads. */
    if (InterlockedIncrement(&activeJobs) > 4) {
        InterlockedDecrement(&activeJobs);
        return false;
    }
    job = (UploadJob*)malloc(sizeof(*job));
    if (!job) goto rejected;
    if (BCryptGenRandom(NULL, randomBytes, sizeof(randomBytes),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        free(job);
        goto rejected;
    }
    for (int i = 0; i < 16; ++i)
        sprintf_s(runId + i * 2, sizeof(runId) - i * 2, "%02x",
            (unsigned)randomBytes[i]);

    /* Integer milliseconds avoid locale-dependent decimal separators. */
    {
        unsigned long long ms = (unsigned long long)((double)seconds * 1000.0);
        length = snprintf(job->json, sizeof(job->json),
            "{\"run_id\":\"%s\",\"score\":%d,\"survival_seconds\":%llu.%03llu,"
            "\"coins\":%d,\"death_cause\":\"%s\"}",
            runId, score, ms / 1000, ms % 1000, coins, cause);
    }
    if (length < 0 || length >= (int)sizeof(job->json)) {
        free(job);
        goto rejected;
    }
    thread = _beginthreadex(NULL, 0, UploadWorker, job, 0, NULL);
    if (!thread) {
        free(job);
        goto rejected;
    }
    CloseHandle((HANDLE)thread); /* Worker continues independently. */
    return true;

rejected:
    InterlockedDecrement(&activeJobs);
    return false;
}

long Telemetry_GetUploadedCount(void)
{
    return InterlockedCompareExchange(&uploaded, 0, 0);
}
long Telemetry_GetFailedCount(void)
{
    return InterlockedCompareExchange(&failed, 0, 0);
}