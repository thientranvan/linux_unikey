#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <libintl.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <ibus.h>

#include "unikey.h"
#include "vnconv.h"

#include "engine_private.h"
#include "unikey_config.h"

#define _(string) gettext(string)

#define CONVERT_BUF_SIZE 1024

static unsigned char WordBreakSyms[] =
{
    ',', ';', ':', '.', '\"', '\'', '!', '?', ' ',
    '<', '>', '=', '+', '-', '*', '/', '\\',
    '_', '~', '`', '@', '#', '$', '%', '^', '&', '(', ')', '{', '}', '[', ']',
    '|'
};

static IBusEngineClass* parent_class = NULL;

static IBusUnikeyEngine* unikey; // current (focus) unikey engine

GType ibus_unikey_engine_get_type(void)
{
    static GType type = 0;

    static const GTypeInfo type_info = {
        sizeof(IBusUnikeyEngineClass),
        (GBaseInitFunc)NULL,
        (GBaseFinalizeFunc)NULL,
        (GClassInitFunc)ibus_unikey_engine_class_init,
        NULL,
        NULL,
        sizeof(IBusUnikeyEngine),
        0,
        (GInstanceInitFunc)ibus_unikey_engine_init,
    };

    if (type == 0)
    {
        type = g_type_register_static(IBUS_TYPE_ENGINE,
                                      "IBusUnikeyEngine",
                                      &type_info,
                                      (GTypeFlags)0);
    }

    return type;
}

void ibus_unikey_init(IBusBus* bus)
{
    UnikeySetup();
    ibus_unikey_config_init();

    ibus_unikey_config_on_changed(ibus_unikey_config_value_changed, NULL);
}

void ibus_unikey_exit()
{
    UnikeyCleanup();
}

static void ibus_unikey_engine_class_init(IBusUnikeyEngineClass* klass)
{
    GObjectClass* object_class         = G_OBJECT_CLASS(klass);
    IBusObjectClass* ibus_object_class = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass* engine_class      = IBUS_ENGINE_CLASS(klass);

    parent_class = (IBusEngineClass* )g_type_class_peek_parent(klass);

    object_class->constructor = ibus_unikey_engine_constructor;
    ibus_object_class->destroy = (IBusObjectDestroyFunc)ibus_unikey_engine_destroy;

    engine_class->process_key_event = ibus_unikey_engine_process_key_event;
    engine_class->reset             = ibus_unikey_engine_reset;
    engine_class->enable            = ibus_unikey_engine_enable;
    engine_class->disable           = ibus_unikey_engine_disable;
    engine_class->focus_in          = ibus_unikey_engine_focus_in;
    engine_class->focus_out         = ibus_unikey_engine_focus_out;
    engine_class->property_activate = ibus_unikey_engine_property_activate;
}

static void ibus_unikey_engine_init(IBusUnikeyEngine* unikey)
{
    ibus_unikey_engine_load_config(unikey);

    UnikeySetInputMethod(unikey->im);
    UnikeySetOutputCharset(unikey->oc);
    UnikeySetOptions(&unikey->ukopt);

    unikey->preeditstr = new std::string();
    unikey->first_word = true;
    ibus_unikey_engine_create_property_list(unikey);
}

static IBusProperty* find_prop_from_list(IBusPropList* list, const char* key)
{
    for (guint i = 0; i < list->properties->len ; i++)
    {
        IBusProperty* prop = ibus_prop_list_get(list, i);
        if (prop == NULL)
            return NULL;
        if (strcmp(ibus_property_get_key(prop), key) == 0)
            return prop;
    }
    return NULL;
}

static void ibus_unikey_engine_update_property_list(IBusUnikeyEngine* unikey)
{
    bool b;
    IBusProperty* prop;

    b = unikey->ukopt.spellCheckEnabled;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_SPELLCHECK);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                (b == 1) ? PROP_STATE_CHECKED:PROP_STATE_UNCHECKED);
    }

    b = unikey->ukopt.autoNonVnRestore;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_AUTORESTORENONVN);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                (b == 1) ? PROP_STATE_CHECKED:PROP_STATE_UNCHECKED);
    }

    b = unikey->ukopt.macroEnabled;
    prop = find_prop_from_list(unikey->prop_list, CONFIG_MACROENABLED);
    if (prop != NULL)
    {
        ibus_property_set_state(prop,
                (b == 1) ? PROP_STATE_CHECKED:PROP_STATE_UNCHECKED);
    }
}

static void ibus_unikey_engine_load_config(IBusUnikeyEngine* unikey)
{
    gchar* str;
    gboolean b;

    auto im = input_method_map.at("telex").first;
    if (ibus_unikey_config_get_string(CONFIG_INPUTMETHOD, &str))
    {
        auto p = input_method_map.at(std::string(str));
        im = p.first;
        g_free(str);
    }
    unikey->im = im;

    auto oc = output_charset_map.at("unicode").first;
    if (ibus_unikey_config_get_string(CONFIG_OUTPUTCHARSET, &str))
    {
        auto p = output_charset_map.at(std::string(str));
        oc = p.first;
        g_free(str);
    }
    unikey->oc = oc;

    if (ibus_unikey_config_get_boolean(CONFIG_FREEMARKING, &b))
        unikey->ukopt.freeMarking = b;

    if (ibus_unikey_config_get_boolean(CONFIG_MODERNSTYLE, &b))
        unikey->ukopt.modernStyle = b;

    if (ibus_unikey_config_get_boolean(CONFIG_MACROENABLED, &b))
        unikey->ukopt.macroEnabled = b;

    if (ibus_unikey_config_get_boolean(CONFIG_SPELLCHECK, &b))
        unikey->ukopt.spellCheckEnabled = b;

    if (ibus_unikey_config_get_boolean(CONFIG_AUTORESTORENONVN, &b))
        unikey->ukopt.autoNonVnRestore = b;

    if (ibus_unikey_config_get_boolean(CONFIG_STANDALONEW, &b))
        unikey->process_w_at_begin = b;

    if (ibus_unikey_config_get_boolean(CONFIG_DIRECTFORWARD, &b))
        unikey->direct_forward = b;

    // load macro
    gchar* fn = get_macro_file();
    UnikeyLoadMacroTable(fn);
    g_free(fn);
}

static GObject* ibus_unikey_engine_constructor(GType type,
                                               guint n_construct_params,
                                               GObjectConstructParam* construct_params)
{
    IBusUnikeyEngine* unikey;

    unikey = (IBusUnikeyEngine*)
        G_OBJECT_CLASS(parent_class)->constructor(type,
                                                  n_construct_params,
                                                  construct_params);

    return (GObject*)unikey;
}

static void ibus_unikey_engine_destroy(IBusUnikeyEngine* unikey)
{
    delete unikey->preeditstr;
    g_object_unref(unikey->prop_list);

    IBUS_OBJECT_CLASS(parent_class)->destroy((IBusObject*)unikey);
}

static void ibus_unikey_buffer_reset(IBusEngine* engine)
{
    unikey = (IBusUnikeyEngine*)engine;

    ibus_engine_hide_preedit_text(engine);
    unikey->preeditstr->clear();
    UnikeyResetBuf();
}

static void ibus_unikey_buffer_commit(IBusEngine* engine)
{
    unikey = (IBusUnikeyEngine*)engine;

    if (!unikey->direct_forward && unikey->preeditstr->length() > 0)
    {
        IBusText *text;
        text = ibus_text_new_from_static_string(unikey->preeditstr->c_str());
        ibus_engine_commit_text(engine, text);
    }

    ibus_unikey_buffer_reset(engine);
}

static void ibus_unikey_engine_focus_in(IBusEngine* engine)
{
    unikey = (IBusUnikeyEngine*)engine;
    ibus_engine_register_properties(engine, unikey->prop_list);

    parent_class->focus_in(engine);
}

static void ibus_unikey_engine_focus_out(IBusEngine* engine)
{
    unikey = (IBusUnikeyEngine*)engine;
    if (unikey->direct_forward
            && unikey->delivery_focus_outs_remaining > 0
            && g_get_monotonic_time() <= unikey->delivery_focus_out_deadline)
    {
        unikey->delivery_focus_outs_remaining--;
        if (unikey->delivery_focus_outs_remaining == 0)
            unikey->delivery_focus_out_deadline = 0;
        parent_class->focus_out(engine);
        return;
    }

    unikey->delivery_focus_out_deadline = 0;
    unikey->delivery_focus_outs_remaining = 0;
    unikey->pending_forwarded_resets = 0;
    unikey->first_word = true;
    ibus_unikey_buffer_reset(engine);
    parent_class->focus_out(engine);
}

static void ibus_unikey_engine_reset(IBusEngine* engine)
{
    unikey = (IBusUnikeyEngine*)engine;
    if (unikey->direct_forward && unikey->pending_forwarded_resets > 0)
    {
        unikey->pending_forwarded_resets--;
        unikey->delivery_focus_out_deadline = g_get_monotonic_time()
                + 100 * G_TIME_SPAN_MILLISECOND;
        unikey->delivery_focus_outs_remaining = 2;
        return;
    }

    if (unikey->direct_forward
            && unikey->delivery_focus_out_deadline > 0
            && g_get_monotonic_time() <= unikey->delivery_focus_out_deadline)
    {
        return;
    }

    ibus_unikey_buffer_reset(engine);
    parent_class->reset(engine);
}

static void ibus_unikey_engine_enable(IBusEngine* engine)
{
    parent_class->enable(engine);
}

static void ibus_unikey_engine_disable(IBusEngine* engine)
{
    parent_class->disable(engine);
}

static void ibus_unikey_config_value_changed(gchar* name, gpointer user_data)
{
    if (unikey == NULL)
        return;

    ibus_unikey_engine_load_config(unikey);

    UnikeySetInputMethod(unikey->im);
    UnikeySetOutputCharset(unikey->oc);
    UnikeySetOptions(&unikey->ukopt);

    ibus_unikey_engine_update_property_list(unikey);
}

static void ibus_unikey_engine_property_activate(IBusEngine* engine,
                                                 const gchar* prop_name,
                                                 guint prop_state)
{
    unikey = (IBusUnikeyEngine*)engine;

    if (strcmp(prop_name, "more-settings") == 0)
    {
        int ret = 0;

        ret = system(LIBEXECDIR "/ibus-setup-unikey &");
        if (ret == -1)
        {
	    g_print("Failed to open ibus-setup-unikey");
        }
        return;
    }

    if (strcmp(prop_name, CONFIG_SPELLCHECK) == 0)
    {
        unikey->ukopt.spellCheckEnabled = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
    else if (strcmp(prop_name, CONFIG_AUTORESTORENONVN) == 0)
    {
        unikey->ukopt.autoNonVnRestore = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
    else if (strcmp(prop_name, CONFIG_MACROENABLED) == 0)
    {
        unikey->ukopt.macroEnabled = prop_state > 0;
        ibus_unikey_config_set_boolean(prop_name, prop_state > 0);
    }
}

static void ibus_unikey_engine_create_property_list(IBusUnikeyEngine* unikey)
{
    IBusProperty* prop;
    IBusText* label;

    if (unikey->prop_list != NULL)
        return;
    unikey->prop_list = ibus_prop_list_new();
    g_object_ref_sink(unikey->prop_list);

    // spellcheck property
    label = ibus_text_new_from_static_string(_("Enable spell check"));
    prop = ibus_property_new(CONFIG_SPELLCHECK,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.spellCheckEnabled==1)?
                             PROP_STATE_CHECKED:PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // auto restore property
    label = ibus_text_new_from_static_string(_("Auto restore non Vietnamese word"));
    prop = ibus_property_new(CONFIG_AUTORESTORENONVN,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.autoNonVnRestore==1)?
                             PROP_STATE_CHECKED:PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // macroEnabled property
    label = ibus_text_new_from_static_string(_("Enable Macro"));
    prop = ibus_property_new(CONFIG_MACROENABLED,
                             PROP_TYPE_TOGGLE,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             (unikey->ukopt.macroEnabled==1)?
                             PROP_STATE_CHECKED:PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);

    // more setting property
    label = ibus_text_new_from_static_string(_("More settings..."));
    prop = ibus_property_new("more-settings",
                             PROP_TYPE_NORMAL,
                             label,
                             "",
                             NULL,
                             TRUE,
                             TRUE,
                             PROP_STATE_UNCHECKED,
                             NULL);
    if (ibus_prop_list_update_property(unikey->prop_list, prop) == false)
        ibus_prop_list_append(unikey->prop_list, prop);
}

static void ibus_unikey_engine_update_preedit_string(IBusEngine *engine, const gchar *string, gboolean visible)
{
    IBusText *text;

    text = ibus_text_new_from_static_string(string);
    ibus_engine_update_preedit_text_with_mode(engine, text, ibus_text_get_length(text), visible, IBUS_ENGINE_PREEDIT_COMMIT);
}

static gboolean ibus_unikey_engine_is_chrome_omnibox(IBusEngine *engine)
{
    static Display *display = XOpenDisplay(NULL);
    if (display == NULL)
        return false;

    Atom active_window_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    Atom actual_type;
    int actual_format;
    unsigned long item_count;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    if (active_window_atom == None
            || XGetWindowProperty(display, DefaultRootWindow(display), active_window_atom,
                    0, 1, False, XA_WINDOW, &actual_type, &actual_format,
                    &item_count, &bytes_after, &data) != Success
            || data == NULL || item_count != 1)
    {
        if (data != NULL)
            XFree(data);
        return false;
    }

    Window window = *(Window*)data;
    XFree(data);

    XClassHint class_hint = {};
    if (!XGetClassHint(display, window, &class_hint))
        return false;
    gchar *window_class = g_ascii_strdown(class_hint.res_class != NULL ? class_hint.res_class : "", -1);
    gboolean chrome = strstr(window_class, "chrome") != NULL;
    g_free(window_class);
    if (class_hint.res_name != NULL)
        XFree(class_hint.res_name);
    if (class_hint.res_class != NULL)
        XFree(class_hint.res_class);
    if (!chrome)
        return false;

    int window_x;
    int window_y;
    Window child;
    if (!XTranslateCoordinates(display, window, DefaultRootWindow(display),
            0, 0, &window_x, &window_y, &child))
        return false;

    gint cursor_y = engine->cursor_area.y - window_y;
    return cursor_y >= 0 && cursor_y < 100;
}

static guint ibus_unikey_ascii_keycode(gunichar character, guint *state)
{
    static const gunichar vietnamese[] = {
        0x00e0, 0x00c0, 0x00e1, 0x00c1, 0x1ea3, 0x1ea2, 0x00e3, 0x00c3,
        0x1ea1, 0x1ea0, 0x0103, 0x0102, 0x1eb1, 0x1eb0, 0x1eaf, 0x1eae,
        0x1eb3, 0x1eb2, 0x1eb5, 0x1eb4, 0x1eb7, 0x1eb6, 0x00e2, 0x00c2,
        0x1ea7, 0x1ea6, 0x1ea5, 0x1ea4, 0x1ea9, 0x1ea8, 0x1eab, 0x1eaa,
        0x1ead, 0x1eac, 0x00e8, 0x00c8, 0x00e9, 0x00c9, 0x1ebb, 0x1eba,
        0x1ebd, 0x1ebc, 0x1eb9, 0x1eb8, 0x00ea, 0x00ca, 0x1ec1, 0x1ec0,
        0x1ebf, 0x1ebe, 0x1ec3, 0x1ec2, 0x1ec5, 0x1ec4, 0x1ec7, 0x1ec6,
        0x00ec, 0x00cc, 0x00ed, 0x00cd, 0x1ec9, 0x1ec8, 0x0129, 0x0128,
        0x1ecb, 0x1eca, 0x00f2, 0x00d2, 0x00f3, 0x00d3, 0x1ecf, 0x1ece,
        0x00f5, 0x00d5, 0x1ecd, 0x1ecc, 0x00f4, 0x00d4, 0x1ed3, 0x1ed2,
        0x1ed1, 0x1ed0, 0x1ed5, 0x1ed4, 0x1ed7, 0x1ed6, 0x1ed9, 0x1ed8,
        0x01a1, 0x01a0, 0x1edd, 0x1edc, 0x1edb, 0x1eda, 0x1edf, 0x1ede,
        0x1ee1, 0x1ee0, 0x1ee3, 0x1ee2, 0x00f9, 0x00d9, 0x00fa, 0x00da,
        0x1ee7, 0x1ee6, 0x0169, 0x0168, 0x1ee5, 0x1ee4, 0x01b0, 0x01af,
        0x1eeb, 0x1eea, 0x1ee9, 0x1ee8, 0x1eed, 0x1eec, 0x1eef, 0x1eee,
        0x1ef1, 0x1ef0, 0x1ef3, 0x1ef2, 0x00fd, 0x00dd, 0x1ef7, 0x1ef6,
        0x1ef9, 0x1ef8, 0x1ef5, 0x1ef4, 0x0111, 0x0110,
    };
    // ponytail: X11-only reserved XKB slots; add Wayland text-input backend before enabling Wayland sessions.
    static const guint vietnamese_keycodes[] = {
        89, 95, 112, 124, 141, 146, 160, 170, 175,
        176, 189, 194, 209, 211, 214, 222, 121,
    };
    static const guint vietnamese_states[] = {
        0,
        IBUS_SHIFT_MASK,
        IBUS_MOD5_MASK,
        IBUS_SHIFT_MASK | IBUS_MOD5_MASK,
        IBUS_MOD3_MASK,
        IBUS_SHIFT_MASK | IBUS_MOD3_MASK,
        IBUS_MOD3_MASK | IBUS_MOD5_MASK,
        IBUS_SHIFT_MASK | IBUS_MOD3_MASK | IBUS_MOD5_MASK,
    };
    static const char unshifted[] = "`1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./ ";
    static const char shifted[] = "~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>? ";
    static const guint ascii_keycodes[] = {
        KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL,
        KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH,
        KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE,
        KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_SPACE,
    };
    static_assert(G_N_ELEMENTS(vietnamese) <= G_N_ELEMENTS(vietnamese_keycodes) * 8, "Vietnamese XKB map too small");
    static_assert(sizeof(unshifted) - 1 == G_N_ELEMENTS(ascii_keycodes), "US keymap size mismatch");
    static_assert(sizeof(shifted) - 1 == G_N_ELEMENTS(ascii_keycodes), "US shifted keymap size mismatch");

    if (character > 0x7f)
    {
        for (guint i = 0; i < G_N_ELEMENTS(vietnamese); i++)
        {
            if (vietnamese[i] == character)
            {
                *state = vietnamese_states[i % G_N_ELEMENTS(vietnamese_states)];
                return vietnamese_keycodes[i / G_N_ELEMENTS(vietnamese_states)];
            }
        }
    }

    *state = 0;
    if (character > 0x7f)
        return 0;

    const char *found = strchr(unshifted, character);
    if (found != NULL)
        return ascii_keycodes[found - unshifted];

    found = strchr(shifted, character);
    if (found != NULL)
    {
        *state = IBUS_SHIFT_MASK;
        return ascii_keycodes[found - shifted];
    }

    return 0;
}

static void ibus_unikey_engine_forward_string(IBusEngine *engine, const std::string& string)
{
    const gchar *current = string.c_str();
    const gchar *end = current + string.length();

    while (current < end)
    {
        gunichar character = g_utf8_get_char(current);
        guint keyval = ibus_unicode_to_keyval(character);
        guint state;
        guint keycode = ibus_unikey_ascii_keycode(character, &state);
        if (character > 0x7f)
            unikey->pending_forwarded_resets++;
        ibus_engine_forward_key_event(engine, keyval, keycode, state);
        ibus_engine_forward_key_event(engine, keyval, keycode, state | IBUS_RELEASE_MASK);
        current = g_utf8_next_char(current);
    }
}

static void ibus_unikey_engine_forward_backspaces(IBusEngine *engine, guint count)
{
    // ponytail: current IBus backend uses Linux evdev keycodes; derive backend keycodes if session stops using evdev.
    while (count-- > 0)
    {
        ibus_engine_forward_key_event(engine, IBUS_BackSpace, KEY_BACKSPACE, 0);
        ibus_engine_forward_key_event(engine, IBUS_BackSpace, KEY_BACKSPACE, IBUS_RELEASE_MASK);
    }
}

static void ibus_unikey_engine_erase_chars(IBusEngine *engine, int count)
{
    int i = unikey->preeditstr->length();

    while (i > 0 && count > 0) {
        unsigned char code = unikey->preeditstr->at(i-1);

        // count down if code is the first byte of utf-8 char
        // REF: http://en.wikipedia.org/wiki/UTF-8
        if (code >> 6 != 2) { // ignore 10xxxxxx
            count--;
        }
        i--;
    }
    unikey->preeditstr->erase(i);
}

// code from x-unikey, for convert charset that not is XUtf-8
int latinToUtf(unsigned char* dst, unsigned char* src, int inSize, int* pOutSize)
{
    int i;
    int outLeft;
    unsigned char ch;

    outLeft = *pOutSize;

    for (i=0; i<inSize; i++)
    {
        ch = *src++;
        if (ch < 0x80)
        {
            outLeft -= 1;
            if (outLeft >= 0)
                *dst++ = ch;
        }
        else
        {
            outLeft -= 2;
            if (outLeft >= 0)
            {
                *dst++ = (0xC0 | ch >> 6);
                *dst++ = (0x80 | (ch & 0x3F));
            }
        }
    }

    *pOutSize = outLeft;
    return (outLeft >= 0);
}


static gboolean ibus_unikey_engine_process_key_event(IBusEngine* engine,
                                                     guint keyval,
                                                     guint keycode,
                                                     guint modifiers)
{
    static gboolean tmp;

    unikey = (IBusUnikeyEngine*)engine;

    tmp = ibus_unikey_engine_process_key_event_preedit(engine, keyval, keycode, modifiers);

    // check last keyevent with shift
    if (keyval >= IBUS_space && keyval <=IBUS_asciitilde)
    {
        unikey->last_key_with_shift = modifiers & IBUS_SHIFT_MASK;
    }
    else
    {
        unikey->last_key_with_shift = false;
    } // end check last keyevent with shift

    return tmp;
}

static gboolean ibus_unikey_engine_process_key_event_preedit(IBusEngine* engine,
                                                             guint keyval,
                                                             guint keycode,
                                                             guint modifiers)
{
    if (modifiers & IBUS_RELEASE_MASK)
    {
        return false;
    }

    if (modifiers & IBUS_CONTROL_MASK
             || modifiers & IBUS_MOD1_MASK // alternate mask
             || keyval == IBUS_Control_L
             || keyval == IBUS_Control_R
             || keyval == IBUS_Tab
             || keyval == IBUS_Return
             || keyval == IBUS_Delete
             || keyval == IBUS_KP_Enter
             || (keyval >= IBUS_Home && keyval <= IBUS_Insert)
             || (keyval >= IBUS_KP_Home && keyval <= IBUS_KP_Delete)
        )
    {
        ibus_unikey_buffer_commit(engine);
        return false;
    }

    else if ((keyval >= IBUS_Caps_Lock && keyval <= IBUS_Hyper_R)
            || (!(modifiers & IBUS_SHIFT_MASK) && (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R))  // when press one shift key
        )
    {
        return false;
    }

    // capture BackSpace
    else if (keyval == IBUS_BackSpace)
    {
        UnikeyBackspacePress();

        if (UnikeyBackspaces == 0 || unikey->preeditstr->empty())
        {
            return false;
        }
        else
        {
            if (unikey->direct_forward)
            {
                guint backspaces = MIN((guint)UnikeyBackspaces,
                        (guint)g_utf8_strlen(unikey->preeditstr->c_str(), -1));
                ibus_unikey_engine_forward_backspaces(engine, backspaces);
                ibus_unikey_engine_erase_chars(engine, backspaces);
            }
            else if (unikey->preeditstr->length() <= (guint)UnikeyBackspaces)
            {
                ibus_unikey_buffer_reset(engine);
            }
            else
            {
                ibus_unikey_engine_erase_chars(engine, UnikeyBackspaces);
                ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), true);
            }

            // change tone position after press backspace
            if (UnikeyBufChars > 0)
            {
                std::string::size_type output_start = unikey->preeditstr->length();
                if (unikey->oc == CONV_CHARSET_XUTF8)
                {
                    unikey->preeditstr->append((const gchar*)UnikeyBuf, UnikeyBufChars);
                }
                else
                {
                    static unsigned char buf[CONVERT_BUF_SIZE];
                    int bufSize = CONVERT_BUF_SIZE;

                    latinToUtf(buf, UnikeyBuf, UnikeyBufChars, &bufSize);
                    unikey->preeditstr->append((const gchar*)buf, CONVERT_BUF_SIZE - bufSize);
                }

                if (unikey->direct_forward)
                    ibus_unikey_engine_forward_string(engine, unikey->preeditstr->substr(output_start));
                else
                    ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), true);
            }
        }
        return true;
    } // end capture BackSpace

    else if (keyval >=IBUS_KP_Multiply && keyval <=IBUS_KP_9)
    {
        ibus_unikey_buffer_commit(engine);
        return false;
    }

    // capture ascii printable char
    else if ((keyval >= IBUS_space && keyval <=IBUS_asciitilde)
            || keyval == IBUS_Shift_L || keyval == IBUS_Shift_R) // sure this have IBUS_SHIFT_MASK
    {
        UnikeySetCapsState(modifiers & IBUS_SHIFT_MASK, modifiers & IBUS_LOCK_MASK);

        // process keyval

        if ((unikey->im == UkTelex || unikey->im == UkSimpleTelex2)
            && unikey->process_w_at_begin == false
            && UnikeyAtWordBeginning()
            && (keyval == IBUS_w || keyval == IBUS_W))
        {
            UnikeyPutChar(keyval);
            std::string output(keyval==IBUS_w?"w":"W");
            unikey->preeditstr->append(output);
            if (unikey->direct_forward)
                ibus_unikey_engine_forward_string(engine, output);
            else
                ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), true);
            return true;
        }

        // shift + space, shift + shift event
        if ((unikey->last_key_with_shift == false && modifiers & IBUS_SHIFT_MASK
                    && keyval == IBUS_space && !UnikeyAtWordBeginning())
            || (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R) // (&& modifiers & IBUS_SHIFT_MASK), sure this have IBUS_SHIFT_MASK
           )
        {
            UnikeyRestoreKeyStrokes();
        } // end shift + space, shift + shift event

        else
        {
            UnikeyFilter(keyval);
        }
        // end process keyval

        // process result of ukengine
        if (UnikeyBackspaces > 0)
        {
            if (unikey->direct_forward)
            {
                guint backspaces = MIN((guint)UnikeyBackspaces,
                        (guint)g_utf8_strlen(unikey->preeditstr->c_str(), -1));
                guint forwarded_backspaces = backspaces;
                if (unikey->first_word
                        && backspaces == (guint)g_utf8_strlen(unikey->preeditstr->c_str(), -1)
                        && ibus_unikey_engine_is_chrome_omnibox(engine))
                {
                    forwarded_backspaces++;
                    unikey->pending_forwarded_resets++;
                }
                ibus_unikey_engine_forward_backspaces(engine, forwarded_backspaces);
                ibus_unikey_engine_erase_chars(engine, backspaces);
            }
            else if (unikey->preeditstr->length() <= (guint)UnikeyBackspaces)
            {
                unikey->preeditstr->clear();
            }
            else
            {
                ibus_unikey_engine_erase_chars(engine, UnikeyBackspaces);
            }
        }

        std::string::size_type output_start = unikey->preeditstr->length();
        if (UnikeyBufChars > 0)
        {
            if (unikey->oc == CONV_CHARSET_XUTF8)
            {
                unikey->preeditstr->append((const gchar*)UnikeyBuf, UnikeyBufChars);
            }
            else
            {
                static unsigned char buf[CONVERT_BUF_SIZE];
                int bufSize = CONVERT_BUF_SIZE;

                latinToUtf(buf, UnikeyBuf, UnikeyBufChars, &bufSize);
                unikey->preeditstr->append((const gchar*)buf, CONVERT_BUF_SIZE - bufSize);
            }
        }
        else if (keyval != IBUS_Shift_L && keyval != IBUS_Shift_R) // if ukengine not process
        {
            static int n;
            static char s[6];

            n = g_unichar_to_utf8(keyval, s); // convert ucs4 to utf8 char
            unikey->preeditstr->append(s, n);
        }
        if (unikey->direct_forward)
            ibus_unikey_engine_forward_string(engine, unikey->preeditstr->substr(output_start));
        // end process result of ukengine

        // commit string: if need
        if (unikey->preeditstr->length() > 0)
        {
            static guint i;
            for (i = 0; i < sizeof(WordBreakSyms); i++)
            {
                if (WordBreakSyms[i] == unikey->preeditstr->at(unikey->preeditstr->length()-1)
                    && WordBreakSyms[i] == keyval)
                {
                    unikey->first_word = false;
                    ibus_unikey_buffer_commit(engine);
                    return true;
                }
            }
        }
        // end commit string

        if (!unikey->direct_forward)
            ibus_unikey_engine_update_preedit_string(engine, unikey->preeditstr->c_str(), true);
        return true;
    } //end capture printable char

    // non process key
    ibus_unikey_buffer_commit(engine);
    return false;
}
