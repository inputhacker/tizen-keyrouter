#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"
#include <tizen-extension-server-protocol.h>
#ifdef ENABLE_CYNARA
#include <cynara-session.h>
#include <cynara-client.h>
#include <cynara-creds-socket.h>
#endif
#include <string.h>

#ifdef TRACE_INPUT_BEGIN
#undef TRACE_INPUT_BEGIN
#endif
#ifdef TRACE_INPUT_END
#undef TRACE_INPUT_END
#endif

#ifdef ENABLE_TTRACE
#include <ttrace.h>

#define TRACE_INPUT_BEGIN(NAME, ...) traceBegin(TTRACE_TAG_INPUT, "INPUT:KRT:"#NAME, ##__VA_ARGS__)
#define TRACE_INPUT_END() traceEnd(TTRACE_TAG_INPUT)
#else
#define TRACE_INPUT_BEGIN(NAME)
#define TRACE_INPUT_END()
#endif

/* Temporary value of maximum number of HWKeys */

#define CHECK_ERR(val) if (TIZEN_KEYROUTER_ERROR_NONE != val) return;
#define CHECK_ERR_VAL(val) if (TIZEN_KEYROUTER_ERROR_NONE != val) return val;
#define CHECK_NULL(val) if (!val) return;
#define CHECK_NULL_VAL(val) if (!val) return val;

extern int _keyrouter_log_dom;

#undef ERR
#undef WRN
#undef INF
#undef DBG
#define ERR(...) EINA_LOG_DOM_ERR(_keyrouter_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_keyrouter_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_keyrouter_log_dom, __VA_ARGS__)
#define DBG(...) EINA_LOG_DOM_DBG(_keyrouter_log_dom, __VA_ARGS__)

#define KLERR(msg, ARG...) ERR(msg, ##ARG)
#define KLWRN(msg, ARG...) WRN(msg, ##ARG)
#define KLINF(msg, ARG...) INF(msg, ##ARG)
#define KLDBG(msg, ARG...) DBG(msg, ##ARG)

typedef struct _E_Keyrouter E_Keyrouter;
typedef struct _E_Keyrouter* E_KeyrouterPtr;
typedef struct _E_Keyrouter_Key_List_Node E_Keyrouter_Key_List_Node;
typedef struct _E_Keyrouter_Key_List_Node* E_Keyrouter_Key_List_NodePtr;
typedef struct _E_Keyrouter_Tizen_HWKey E_Keyrouter_Tizen_HWKey;
typedef struct _E_Keyrouter_Grabbed_Key E_Keyrouter_Grabbed_Key;
typedef struct _E_Keyrouter_Grab_Request E_Keyrouter_Grab_Request;
typedef struct _E_Keyrouter_Ungrab_Request E_Keyrouter_Ungrab_Request;
typedef struct _E_Keyrouter_Registered_Window_Info E_Keyrouter_Registered_Window_Info;

typedef struct _E_Keyrouter_Conf_Edd E_Keyrouter_Conf_Edd;
typedef struct _E_Keyrouter_Config_Data E_Keyrouter_Config_Data;

#define TIZEN_KEYROUTER_MODE_PRESSED        TIZEN_KEYROUTER_MODE_REGISTERED+1
#define TIZEN_KEYROUTER_MODE_PICTURE_OFF        TIZEN_KEYROUTER_MODE_REGISTERED+2

typedef enum _E_Keyrouter_Client_Status
{
   E_KRT_CSTAT_DEAD = 0,
   E_KRT_CSTAT_ALIVE,
   E_KRT_CSTAT_UNGRAB
} E_Keyrouter_Client_Status;

typedef unsigned long Time;

extern E_KeyrouterPtr krt;

struct _E_Keyrouter_Conf_Edd
{
   int num_keycode;
   int max_keycode;
   int pictureoff_disabled;
   Eina_List *KeyList;
};

struct _E_Keyrouter_Config_Data
{
   E_Module *module;
   E_Config_DD *conf_edd;
   E_Config_DD *conf_hwkeys_edd;
   E_Keyrouter_Conf_Edd *conf;
};

struct _E_Keyrouter_Registered_Window_Info
{
   struct wl_resource *surface;
   Eina_List *keys;
};

struct _E_Keyrouter_Key_List_Node
{
   struct wl_resource *surface;
   struct wl_client *wc;
   Eina_Bool focused;
   E_Keyrouter_Client_Status status;
};

struct _E_Keyrouter_Tizen_HWKey
{
   char *name;
   int keycode;
   int no_privcheck;
   int repeat;
};

struct _E_Keyrouter_Grabbed_Key
{
   int keycode;
   char* keyname;
   Eina_Bool no_privcheck;
   Eina_Bool repeat;

   Eina_List *excl_ptr;
   Eina_List *or_excl_ptr;
   Eina_List *top_ptr;
   Eina_List *shared_ptr;
   Eina_List *press_ptr;
   E_Keyrouter_Key_List_Node *registered_ptr;
   Eina_List *pic_off_ptr;
};

struct _E_Keyrouter
{
   struct wl_global *global;
   Ecore_Event_Filter *ef_handler;
   Eina_List *handlers;
   Eina_List *resources;

   E_Keyrouter_Config_Data *conf;

   E_Keyrouter_Grabbed_Key *HardKeys;
   Eina_List *grab_surface_list;
   Eina_List *grab_client_list;

   Eina_List *registered_window_list;

   Eina_Bool isWindowStackChanged;
   int numTizenHWKeys;
   int max_tizen_hwkeys;
   int register_none_key;
   Eina_List *registered_none_key_window_list;
   Eina_List *invisible_set_window_list;
   Eina_List *invisible_get_window_list;
   struct wl_resource * playback_daemon_surface;
#ifdef ENABLE_CYNARA
   cynara *p_cynara;
#endif
   int isPictureOffEnabled;
   Eina_Bool pictureoff_disabled;
};

struct _E_Keyrouter_Grab_Request {
   int key;
   int mode;
   int err;
};

struct _E_Keyrouter_Ungrab_Request {
   int key;
   int err;
};


/* E Module */
E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

int e_keyrouter_set_keygrab_in_list(struct wl_resource *surface, struct wl_client *client, uint32_t key, uint32_t mode);
int e_keyrouter_prepend_to_keylist(struct wl_resource *surface, struct wl_client *wc, uint32_t key, uint32_t mode, Eina_Bool focused);
void e_keyrouter_find_and_remove_client_from_list(struct wl_resource *surface, struct wl_client *wc, uint32_t key, uint32_t mode);
void e_keyrouter_remove_client_from_list(struct wl_resource *surface, struct wl_client *wc);
int e_keyrouter_find_key_in_list(struct wl_resource *surface, struct wl_client *wc, uint32_t key);
Eina_Bool e_keyrouter_find_key_in_register_list(uint32_t key);

int e_keyrouter_add_client_destroy_listener(struct wl_client *client);
int e_keyrouter_add_surface_destroy_listener(struct wl_resource *surface);

Eina_Bool e_keyrouter_process_key_event(void *event, int type);

int e_keyrouter_set_keyregister(struct wl_client *client, struct wl_resource *surface, uint32_t key);
int e_keyrouter_unset_keyregister(struct wl_resource *surface, struct wl_client *client, uint32_t key);
Eina_Bool e_keyrouter_is_registered_window(struct wl_resource *surface);
void e_keyrouter_clear_registered_window(void);
Eina_List* _e_keyrouter_registered_window_key_list(struct wl_resource *surface);
Eina_Bool IsNoneKeyRegisterWindow(struct wl_resource *surface);
Eina_Bool IsInvisibleSetWindow(struct wl_resource *surface);
Eina_Bool IsInvisibleGetWindow(struct wl_resource *surface);


struct wl_resource *e_keyrouter_util_get_surface_from_eclient(E_Client *client);
int e_keyrouter_util_get_pid(struct wl_client *client, struct wl_resource *surface);
char *e_keyrouter_util_cmd_get_from_pid(int pid);
int e_keyrouter_util_keycode_get_from_string(char *name);
char *e_keyrouter_util_keyname_get_from_keycode(int keycode);
char *e_keyrouter_util_process_name_get_from_cmd(char *cmd);
const char *e_keyrouter_mode_to_string(uint32_t mode);

void e_keyrouter_conf_init(E_Keyrouter_Config_Data *kconfig);
void e_keyrouter_conf_deinit(E_Keyrouter_Config_Data *kconfig);
int e_keyrouter_cb_picture_off(const int option, void *data);

#endif
