#define _CRT_SECURE_NO_WARNINGS
#include "pch.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

// --- ENDERECOS DOS HOOKS ---
// cBaseGame::RankPlayers (Stroke / VS)
#define ADDR_BASEGAME_XP_SAVE      0x0042b816
#define RET_BASEGAME_XP_SAVE       0x0042b81c

// c30BattleGame::CalcExp (Tournament)
#define ADDR_TOURNAMENT_XP_SAVE1   0x00428969
#define RET_TOURNAMENT_XP_SAVE1    0x0042896f
#define ADDR_TOURNAMENT_XP_SAVE2   0x004289b1
#define RET_TOURNAMENT_XP_SAVE2    0x004289b7
#define ADDR_TOURNAMENT_XP_SAVE3   0x004289c5
#define RET_TOURNAMENT_XP_SAVE3    0x004289cb

// cMatchGame::RankPlayers (Match)
#define ADDR_MATCHGAME_XP_SAVE     0x0042df0c
#define RET_MATCHGAME_XP_SAVE      0x0042df12

// cTeamGame::RankPlayers (Team)
#define ADDR_TEAMGAME_XP_SAVE      0x0042fe72
#define RET_TEAMGAME_XP_SAVE       0x0042fe78

int g_xpMultiplier = 10;

void Log(const char* format, ...) {
    FILE* f = NULL;
    fopen_s(&f, "CalcExpHook.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        va_list args;
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

void ReadConfig() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strcat_s(path, "Server.ini");
    } else {
        strcpy_s(path, ".\\Server.ini");
    }
    g_xpMultiplier = (int)GetPrivateProfileIntA("OPTION", "XP_MULTIPLIER", 10, path);
    Log("Config: XP_MULTIPLIER = %d", g_xpMultiplier);
}

// --- LOGICA DE MULTIPLICACAO ---

uint32_t __stdcall MultiplyValue(uint32_t baseXP) {
    if (g_xpMultiplier <= 1 || baseXP == 0) return baseXP;
    if (baseXP > 5000) return baseXP; // Protecao

    uint32_t newXP = baseXP * (uint32_t)g_xpMultiplier;
    Log("XP Multiplicado: %u -> %u", baseXP, newXP);
    return newXP;
}

// --- HOOKS ---

// Hook para BaseGame (Stroke/VS) - Corrige a barra de XP tambem
void __declspec(naked) BaseGameRankPlayersHook() {
    __asm {
        pushad
        push eax
        call MultiplyValue
        mov [esp + 0x1C], eax // Atualiza EAX no pushad
        popad

        // Corrige a exibicao na barra de XP (stack buffer +7 offset do inicio do buffer)
        mov word ptr [esp + 0x23], ax 

        // Instrucao original: MOV dword ptr [ECX + 0xb0], EAX
        mov dword ptr [ecx + 0xb0], eax
        
        push RET_BASEGAME_XP_SAVE
        ret
    }
}

// Hooks para Tournament (30BattleGame)
void __declspec(naked) TournamentCalcExpHook1() {
    __asm {
        pushad
        push eax
        call MultiplyValue
        mov [esp + 0x1C], eax
        popad
        mov dword ptr [ecx + 0xb0], eax
        push RET_TOURNAMENT_XP_SAVE1
        ret
    }
}

void __declspec(naked) TournamentCalcExpHook2() {
    __asm {
        pushad
        push eax
        call MultiplyValue
        mov [esp + 0x1C], eax
        popad
        mov dword ptr [ecx + 0xb0], eax
        push RET_TOURNAMENT_XP_SAVE2
        ret
    }
}

void __declspec(naked) TournamentCalcExpHook3() {
    __asm {
        pushad
        push edx // XP esta em EDX aqui
        call MultiplyValue
        mov [esp + 0x14], eax // Atualiza EDX no pushad
        popad
        mov dword ptr [ecx + 0xb0], edx
        push RET_TOURNAMENT_XP_SAVE3
        ret
    }
}

// Hook para Match Game
void __declspec(naked) MatchGameRankPlayersHook() {
    __asm {
        pushad
        push eax
        call MultiplyValue
        mov [esp + 0x1C], eax
        popad
        mov dword ptr [edi + 0xb0], eax // EDI e a sessao aqui
        push RET_MATCHGAME_XP_SAVE
        ret
    }
}

// Hook para Team Game
void __declspec(naked) TeamGameRankPlayersHook() {
    __asm {
        pushad
        push ecx // XP esta em ECX aqui
        call MultiplyValue
        mov [esp + 0x18], eax // Atualiza ECX no pushad
        popad

        // Corrige a exibicao na barra de XP
        mov word ptr [esp + 0x1f], cx

        mov dword ptr [eax + 0xb0], ecx // EAX e a sessao aqui
        push RET_TEAMGAME_XP_SAVE
        ret
    }
}

// --- UTILITARIOS ---

void WriteJmp(DWORD addr, void* hook, int nops = 0) {
    DWORD oldProtect;
    if (VirtualProtect((void*)addr, 5 + nops, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *(uint8_t*)addr = 0xE9;
        *(uint32_t*)(addr + 1) = (uint32_t)hook - addr - 5;
        for (int i = 0; i < nops; i++) *(uint8_t*)(addr + 5 + i) = 0x90;
        VirtualProtect((void*)addr, 5 + nops, oldProtect, &oldProtect);
    }
}

void ApplyHooks() {
    ReadConfig();
    
    // Base Game / Stroke / VS
    WriteJmp(ADDR_BASEGAME_XP_SAVE, BaseGameRankPlayersHook, 1);
    Log("Hook BaseGame aplicado em 0x%X", ADDR_BASEGAME_XP_SAVE);

    // Tournament
    WriteJmp(ADDR_TOURNAMENT_XP_SAVE1, TournamentCalcExpHook1, 1);
    WriteJmp(ADDR_TOURNAMENT_XP_SAVE2, TournamentCalcExpHook2, 1);
    WriteJmp(ADDR_TOURNAMENT_XP_SAVE3, TournamentCalcExpHook3, 1);
    Log("Hooks Tournament aplicados");

    // Match
    WriteJmp(ADDR_MATCHGAME_XP_SAVE, MatchGameRankPlayersHook, 1);
    Log("Hook MatchGame aplicado em 0x%X", ADDR_MATCHGAME_XP_SAVE);

    // Team
    WriteJmp(ADDR_TEAMGAME_XP_SAVE, TeamGameRankPlayersHook, 1);
    Log("Hook TeamGame aplicado em 0x%X", ADDR_TEAMGAME_XP_SAVE);
    
    Log("Todos os hooks de XP inicializados.");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        ApplyHooks();
    }
    return TRUE;
}
