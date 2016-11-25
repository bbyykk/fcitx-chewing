/***************************************************************************
 *   Copyright (C) 2012~2012 by Tai-Lin Chu                                *
 *   tailinchu@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcitx/ime.h>
#include <fcitx-config/fcitx-config.h>
#include <fcitx-config/xdg.h>
#include <fcitx-config/hotkey.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utils.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/instance.h>
#include <fcitx/context.h>
#include <fcitx/keys.h>
#include <fcitx/ui.h>
#include <libintl.h>

#include <taigi.h>

#include "config.h"
#include "eim.h"

CONFIG_DESC_DEFINE(GetFcitxChewingConfigDesc, "fcitx-taigi.desc")
static int FcitxChewingGetRawCursorPos(char * str, int upos);
static INPUT_RETURN_VALUE FcitxChewingGetCandWord(void* arg, FcitxCandidateWord* candWord);
static void FcitxChewingReloadConfig(void* arg);
static boolean LoadChewingConfig(FcitxChewingConfig* fs);
static void SaveChewingConfig(FcitxChewingConfig* fs);
static void ConfigChewing(FcitxChewing* chewing);
static void FcitxChewingOnClose(void* arg, FcitxIMCloseEventType event);
static INPUT_RETURN_VALUE FcitxChewingKeyBlocker(void* arg, FcitxKeySym key, unsigned int state);

typedef struct _ChewingCandWord {
    int index;
} ChewingCandWord;

FCITX_DEFINE_PLUGIN(fcitx_taigi, ime2, FcitxIMClass2) = {
    FcitxChewingCreate,
    FcitxChewingDestroy,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

const FcitxHotkey FCITX_CHEWING_SHIFT_LEFT[2] = {{NULL, FcitxKey_Left, FcitxKeyState_Shift}, {NULL, FcitxKey_None, FcitxKeyState_None}};
const FcitxHotkey FCITX_CHEWING_SHIFT_RIGHT[2] = {{NULL, FcitxKey_Right, FcitxKeyState_Shift}, {NULL, FcitxKey_None, FcitxKeyState_None}};
const FcitxHotkey FCITX_CHEWING_UP[2] = {{NULL, FcitxKey_Up, FcitxKeyState_None}, {NULL, FcitxKey_None, FcitxKeyState_None}};
const FcitxHotkey FCITX_CHEWING_DOWN[2] = {{NULL, FcitxKey_Down, FcitxKeyState_None}, {NULL, FcitxKey_None, FcitxKeyState_None}};
const FcitxHotkey FCITX_CHEWING_PGUP[2] = {{NULL, FcitxKey_Page_Up, FcitxKeyState_None}, {NULL, FcitxKey_None, FcitxKeyState_None}};
const FcitxHotkey FCITX_CHEWING_PGDN[2] = {{NULL, FcitxKey_Page_Down, FcitxKeyState_None}, {NULL, FcitxKey_None, FcitxKeyState_None}};

const char *builtin_keymaps[] = {
    "KB_DEFAULT",
    "KB_HSU",
    "KB_IBM",
    "KB_GIN_YEIH",
    "KB_ET",
    "KB_ET26",
    "KB_DVORAK",
    "KB_DVORAK_HSU",
    "KB_DACHEN_CP26",
    "KB_HANYU_PINYIN"
};

#ifdef FcitxLog
//#undef FcitxLog
//#define FcitxLog(e, fmt...)
#endif

void logger(void *data, int level, const char *fmt, ...)
{
	va_list ap;
	FILE *fd = (FILE *) data;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	//fprintf(fd, fmt, ap);
	va_end(ap);
}


/**
 * @brief initialize the extra input method
 *
 * @param arg
 * @return successful or not
 **/
void* FcitxChewingCreate(FcitxInstance* instance)
{
    if (GetFcitxChewingConfigDesc() == NULL)
        return NULL;
    
    char* user_path = NULL;
    FILE* fp = FcitxXDGGetFileUserWithPrefix("taigi", ".place_holder", "w", NULL);
    if (fp)
        fclose(fp);
    FcitxXDGGetFileUserWithPrefix("taigi", "", NULL, &user_path);
    FcitxLog(INFO, "Taigi storage path %s", user_path);
    
    FcitxChewing* chewing = (FcitxChewing*) fcitx_utils_malloc0(sizeof(FcitxChewing));
    FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(instance);
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    
    bindtextdomain("fcitx-taigi", LOCALEDIR);
    bind_textdomain_codeset("fcitx-taigi", "UTF-8");

    chewing->context = taigi_new();
    ChewingContext * ctx = chewing->context;
    
    if (NULL == chewing->context) {
        FcitxLog(DEBUG, "taigi init failed");
        free(chewing);
        return NULL;
    } else {
        FcitxLog(DEBUG, "taigi init ok");
    }
    {
	    void *p = NULL;
	    taigi_set_logger(ctx, logger, p); 
    }
    chewing->owner = instance;
    taigi_set_maxChiSymbolLen(ctx, CHEWING_MAX_LEN);
    // chewing will crash without set page
    taigi_set_candPerPage(ctx, config->iMaxCandWord);
    FcitxCandidateWordSetPageSize(FcitxInputStateGetCandidateList(input), config->iMaxCandWord);
    LoadChewingConfig(&chewing->config);
    ConfigChewing(chewing);

    FcitxIMIFace iface;
    memset(&iface, 0, sizeof(FcitxIMIFace));

    iface.Init = FcitxChewingInit;
    iface.ResetIM = FcitxChewingReset;
    iface.DoInput = FcitxChewingDoInput;
    iface.GetCandWords = FcitxChewingGetCandWords;
    iface.Save = NULL;
    iface.ReloadConfig = FcitxChewingReloadConfig;
    iface.OnClose = FcitxChewingOnClose;
    iface.KeyBlocker = FcitxChewingKeyBlocker;

    FcitxInstanceRegisterIMv2(
        instance,
        chewing,
        "taigi",
        _("Taigi"),
        "taigi",
        iface,
        1,
        "zh_TW"
    );

    return chewing;
}

/**
 * @brief Process Key Input and return the status
 *
 * @param keycode keycode from XKeyEvent
 * @param state state from XKeyEvent
 * @param count count from XKeyEvent
 * @return INPUT_RETURN_VALUE
 **/
INPUT_RETURN_VALUE FcitxChewingDoInput(void* arg, FcitxKeySym sym, unsigned int state)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    ChewingContext* ctx = chewing->context;

    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (FcitxHotkeyIsHotKey(sym, state, FCITX_SPACE)) {
        taigi_handle_Space(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_TAB)) {
        taigi_handle_Tab(ctx);
    } else if (FcitxHotkeyIsHotKeySimple(sym, state)) {
        int scan_code = (int) sym & 0xff;
        taigi_handle_Default(ctx, scan_code);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_BACKSPACE)) {
        const char* zuin_str = taigi_bopomofo_String_static(ctx);
        if (taigi_buffer_Len(ctx) == 0 && !zuin_str[0])
            return IRV_TO_PROCESS;
        taigi_handle_Backspace(ctx);
        if (taigi_buffer_Len(ctx) == 0 && !zuin_str[0])
            return IRV_CLEAN;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ESCAPE)) {
        taigi_handle_Esc(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_DELETE)) {
        const char* zuin_str = taigi_bopomofo_String_static(ctx);
        if (taigi_buffer_Len(ctx) == 0 && !zuin_str[0])
            return IRV_TO_PROCESS;
        taigi_handle_Del(ctx);
        if (taigi_buffer_Len(ctx) == 0 && !zuin_str[0])
            return IRV_CLEAN;
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_UP)) {
        taigi_handle_Up(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_DOWN)) {
        taigi_handle_Down(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_PGUP)) {
        taigi_handle_PageDown(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_PGDN)) {
        taigi_handle_PageUp(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_RIGHT)) {
        taigi_handle_Right(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_LEFT)) {
        taigi_handle_Left(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_HOME)) {
        taigi_handle_Home(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_END)) {
        taigi_handle_End(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_SHIFT_SPACE)) {
        taigi_handle_ShiftSpace(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_SHIFT_LEFT)) {
        taigi_handle_ShiftLeft(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_CHEWING_SHIFT_RIGHT)) {
        taigi_handle_ShiftRight(ctx);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ENTER)) {
        taigi_handle_Enter(ctx);
    } else if (state == FcitxKeyState_Ctrl && FcitxHotkeyIsHotKeyDigit(sym, FcitxKeyState_None)) {
        taigi_handle_CtrlNum(ctx, sym);
    } else {
        // to do: more taigi_handle
        return IRV_TO_PROCESS;
    }

    if (taigi_keystroke_CheckAbsorb(ctx)) {
        return IRV_DISPLAY_CANDWORDS;
    } else if (taigi_keystroke_CheckIgnore(ctx)) {
        return IRV_TO_PROCESS;
    } else if (taigi_commit_Check(ctx)) {
        char* str = taigi_commit_String(ctx);
        FcitxInputContext* ic = FcitxInstanceGetCurrentIC(chewing->owner);
        FcitxInstanceCommitString(chewing->owner, ic, str);
        taigi_free(str);
        return IRV_DISPLAY_CANDWORDS;
    } else {
        return IRV_DISPLAY_CANDWORDS;
    }
}

boolean FcitxChewingInit(void* arg)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    FcitxInstanceSetContext(chewing->owner, CONTEXT_IM_KEYBOARD_LAYOUT, "us");
    FcitxInstanceSetContext(chewing->owner, CONTEXT_ALTERNATIVE_PREVPAGE_KEY, FCITX_LEFT);
    FcitxInstanceSetContext(chewing->owner, CONTEXT_ALTERNATIVE_NEXTPAGE_KEY, FCITX_RIGHT);
    return true;
}

void FcitxChewingReset(void* arg)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    taigi_Reset(chewing->context);

    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    taigi_set_KBType( chewing->context, taigi_KBStr2Num(
                (char *) builtin_keymaps[chewing->config.layout]));

    taigi_set_ChiEngMode(chewing->context, CHINESE_MODE);
#if 0
    FcitxUIStatus* puncStatus = FcitxUIGetStatusByName(chewing->owner, "punc");
    if (puncStatus) {
        if (puncStatus->getCurrentStatus(puncStatus->arg))
            taigi_set_ShapeMode(chewing->context, FULLSHAPE_MODE);
        else
            taigi_set_ShapeMode(chewing->context, HALFSHAPE_MODE);
    }
#endif
}




static boolean FcitxChewingPaging(void* arg, boolean prev)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(chewing->owner);
    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);

    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (FcitxCandidateWordGetListSize(candList) == 0) {
        return false;
    }

    if (prev) {
        taigi_handle_Left(chewing->context);
    } else {
        taigi_handle_Right(chewing->context);
    }

    if (taigi_keystroke_CheckAbsorb(chewing->context)) {
        FcitxChewingGetCandWords(chewing);
        FcitxUIUpdateInputWindow(chewing->owner);
        return true;
    }
    return false;
}

static const char *builtin_selectkeys[] = {
DIGIT_STR_CHOOSE,
"asdfghjkl;",
"asdfzxcv89",
"asdfjkl789",
"aoeuhtn789",
"1234qweras",
};

/**
 * @brief function DoInput has done everything for us.
 *
 * @param searchMode
 * @return INPUT_RETURN_VALUE
 **/
INPUT_RETURN_VALUE FcitxChewingGetCandWords(void* arg)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(chewing->owner);
    FcitxMessages *msgPreedit = FcitxInputStateGetPreedit(input);
    FcitxMessages *clientPreedit = FcitxInputStateGetClientPreedit(input);
    ChewingContext * ctx = chewing->context;
    FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(chewing->owner);
    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);

    int selkey[10];
    int i = 0;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    for (i = 0; i < 10; i++) {
        selkey[i] = builtin_selectkeys[chewing->config.selkey][i];
    }
    
    taigi_set_selKey(ctx, selkey, 10);
    taigi_set_candPerPage(ctx, config->iMaxCandWord);
    FcitxCandidateWordSetPageSize(candList, config->iMaxCandWord);
    FcitxCandidateWordSetChoose(candList, builtin_selectkeys[chewing->config.selkey]);

    //clean up window asap
    FcitxInstanceCleanInputWindow(chewing->owner);

    char * buf_str = taigi_buffer_String(ctx);
    const char* zuin_str = taigi_bopomofo_String_static(ctx);
    ConfigChewing(chewing);

    FcitxLog(DEBUG, "(%s)(%s)", buf_str, zuin_str);

    int index = 0;
    /* if not check done, so there is candidate word */
    if (!taigi_cand_CheckDone(ctx)) {
        //get candidate word
        taigi_cand_Enumerate(ctx);
        while (taigi_cand_hasNext(ctx)) {
            char* str = taigi_cand_String(ctx);
            FcitxCandidateWord cw;
            ChewingCandWord* w = (ChewingCandWord*) fcitx_utils_malloc0(sizeof(ChewingCandWord));
            w->index = index;
            cw.callback = FcitxChewingGetCandWord;
            cw.owner = chewing;
            cw.priv = w;
            cw.strExtra = NULL;
            cw.strWord = strdup(str);
            cw.wordType = MSG_OTHER;
            FcitxCandidateWordAppend(candList, &cw);
            taigi_free(str);
            index ++;
        }

        if (FcitxCandidateWordGetListSize(candList) > 0) {
            FcitxCandidateWordSetOverridePaging(
                candList,
                taigi_cand_CurrentPage(ctx) > 0,
                taigi_cand_CurrentPage(ctx) + 1 < taigi_cand_TotalPage(ctx),
                FcitxChewingPaging,
                chewing,
                NULL);
        }
    }

    do {
        /* there is nothing */
        if (strlen(zuin_str) == 0 && strlen(buf_str) == 0 && index == 0)
            break;

        // setup cursor
        FcitxInputStateSetShowCursor(input, true);
        int cur = taigi_cursor_Current(ctx);
        int rcur = taigi_cursor_Raw(ctx);
        FcitxLog(DEBUG, "cur: %d, rcur: %d", cur, rcur);
        FcitxInputStateSetCursorPos(input, rcur);
        FcitxInputStateSetClientCursorPos(input, rcur);

        // insert zuin in the middle
        char * half1 = strndup(buf_str, rcur);
        char * half2 = strdup(buf_str + rcur);
	
	if(half1)
		FcitxLog(DEBUG, "half1: %s", half1);
	if(half2)
		FcitxLog(DEBUG, "half2: %s", half2);
        FcitxMessagesAddMessageAtLast(msgPreedit, MSG_INPUT, "%s", half1);
        FcitxMessagesAddMessageAtLast(msgPreedit, MSG_CODE, "%s", zuin_str);
        FcitxMessagesAddMessageAtLast(msgPreedit, MSG_INPUT, "%s", half2);

        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_OTHER, "%s", half1);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_HIGHLIGHT | MSG_DONOT_COMMIT_WHEN_UNFOCUS, "%s", zuin_str);
        FcitxMessagesAddMessageAtLast(clientPreedit, MSG_OTHER, "%s", half2);
        free(half1);
        free(half2);
    } while(0);

    taigi_free(buf_str);

    return IRV_DISPLAY_CANDWORDS;
}

INPUT_RETURN_VALUE FcitxChewingGetCandWord(void* arg, FcitxCandidateWord* candWord)
{
    FcitxChewing* chewing = (FcitxChewing*) candWord->owner;
    ChewingCandWord* w = (ChewingCandWord*) candWord->priv;
    FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(chewing->owner);
    FcitxInputState *input = FcitxInstanceGetInputState(chewing->owner);
    int page = w->index / config->iMaxCandWord + taigi_cand_CurrentPage(chewing->context);
    int off = w->index % config->iMaxCandWord;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (page < 0 || page >= taigi_cand_TotalPage(chewing->context))
        return IRV_TO_PROCESS;
    int lastPage = taigi_cand_CurrentPage(chewing->context);
    while (page != taigi_cand_CurrentPage(chewing->context)) {
        if (page < taigi_cand_CurrentPage(chewing->context)) {
            taigi_handle_Left(chewing->context);
        }
        if (page > taigi_cand_CurrentPage(chewing->context)) {
            taigi_handle_Right(chewing->context);
        }
        /* though useless, but take care if there is a bug cause freeze */
        if (lastPage == taigi_cand_CurrentPage(chewing->context)) {
            break;
        }
        lastPage = taigi_cand_CurrentPage(chewing->context);
    }
    taigi_handle_Default( chewing->context, builtin_selectkeys[chewing->config.selkey][off] );
    
    if (taigi_keystroke_CheckAbsorb(chewing->context)) {
        return IRV_DISPLAY_CANDWORDS;
    } else if (taigi_keystroke_CheckIgnore(chewing->context)) {
        return IRV_TO_PROCESS;
    } else if (taigi_commit_Check(chewing->context)) {
        char* str = taigi_commit_String(chewing->context);
        strcpy(FcitxInputStateGetOutputString(input), str);
        taigi_free(str);
        return IRV_COMMIT_STRING;
    } else
        return IRV_DISPLAY_CANDWORDS;
}

/**
 * @brief Get the non-utf8 cursor pos for fcitx
 *
 * @return int
 **/
static int FcitxChewingGetRawCursorPos(char * str, int upos)
{
    unsigned int i;
    int pos = 0;
    for (i = 0; i < upos; i++) {
        pos += fcitx_utf8_char_len(fcitx_utf8_get_nth_char(str, i));
    }
    FcitxLog(INFO, "%s, upos=%d, pos=%d\n", __func__, upos, pos);
    return pos;
}

/**
 * @brief Destroy the input method while unload it.
 *
 * @return int
 **/
void FcitxChewingDestroy(void* arg)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    taigi_delete(chewing->context);
    free(arg);
}

void FcitxChewingReloadConfig(void* arg) {
    FcitxChewing* chewing = (FcitxChewing*) arg;
    LoadChewingConfig(&chewing->config);
    ConfigChewing(chewing);
}

void FcitxChewingOnClose(void* arg, FcitxIMCloseEventType event)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    ChewingContext* ctx = chewing->context;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (event == CET_LostFocus || event == CET_ChangeByInactivate) {
        taigi_handle_Enter(ctx);
        if (event == CET_ChangeByInactivate) {
            if (taigi_commit_Check(ctx)) {
                char* str = taigi_commit_String(ctx);
                FcitxInputContext* ic = FcitxInstanceGetCurrentIC(chewing->owner);
                FcitxInstanceCommitString(chewing->owner, ic, str);
                taigi_free(str);
            } else {
                char * buf_str = taigi_buffer_String(ctx);
                do {
                    /* there is nothing */
                    if (strlen(buf_str) == 0)
                        break;
                    FcitxInputContext* ic = FcitxInstanceGetCurrentIC(chewing->owner);
                    FcitxInstanceCommitString(chewing->owner, ic, buf_str);
                } while(0);
                taigi_free(buf_str);
            }
        }
        else {
            FcitxInstanceResetInput(chewing->owner);
        }
    }

}

INPUT_RETURN_VALUE FcitxChewingKeyBlocker(void* arg, FcitxKeySym key, unsigned int state)
{
    FcitxChewing* chewing = (FcitxChewing*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(chewing->owner);
    FcitxCandidateWordList* candList = FcitxInputStateGetCandidateList(input);
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (FcitxCandidateWordGetListSize(candList) > 0
        && (FcitxHotkeyIsHotKeySimple(key, state)
        || FcitxHotkeyIsHotkeyCursorMove(key, state)
        || FcitxHotkeyIsHotKey(key, state, FCITX_SHIFT_SPACE)
        || FcitxHotkeyIsHotKey(key, state, FCITX_TAB)
        || FcitxHotkeyIsHotKey(key, state, FCITX_SHIFT_ENTER)
        ))
        return IRV_DO_NOTHING;
    else
        return IRV_TO_PROCESS;
}

boolean LoadChewingConfig(FcitxChewingConfig* fs)
{
    FcitxConfigFileDesc *configDesc = GetFcitxChewingConfigDesc();
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    if (!configDesc)
        return false;

    FILE *fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-taigi.config", "r", NULL);

    if (!fp) {
        if (errno == ENOENT)
            SaveChewingConfig(fs);
    }
    FcitxConfigFile *cfile = FcitxConfigParseConfigFileFp(fp, configDesc);

    FcitxChewingConfigConfigBind(fs, cfile, configDesc);
    FcitxConfigBindSync(&fs->config);

    if (fp)
        fclose(fp);
    return true;
}

void SaveChewingConfig(FcitxChewingConfig* fc)
{
    FcitxConfigFileDesc *configDesc = GetFcitxChewingConfigDesc();
    FILE *fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-taigi.config", "w", NULL);
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    FcitxConfigSaveConfigFileFp(fp, &fc->config, configDesc);
    if (fp)
        fclose(fp);
}

void ConfigChewing(FcitxChewing* chewing)
{
    ChewingContext* ctx = chewing->context;
    FcitxLog(INFO, "%s, %d", __func__, __LINE__);
    taigi_set_addPhraseDirection( ctx, chewing->config.bAddPhraseForward ? 0 : 1 );
    taigi_set_phraseChoiceRearward( ctx, chewing->config.bChoiceBackward ? 1 : 0 );
    taigi_set_autoShiftCur( ctx, chewing->config.bAutoShiftCursor ? 1 : 0 );
    taigi_set_spaceAsSelection( ctx, chewing->config.bSpaceAsSelection ? 1 : 0 );
    taigi_set_escCleanAllBuf( ctx, 1 );
}
