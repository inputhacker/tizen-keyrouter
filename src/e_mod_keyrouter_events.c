#ifndef _F_ENABLE_KEYROUTER_CORE

#define E_COMP_WL
#include "e_mod_main_wl.h"

static Eina_Bool _e_keyrouter_send_key_events(int type, Ecore_Event_Key *ev);
static Eina_Bool _e_keyrouter_send_key_events_press(int type, Ecore_Event_Key *ev);
static Eina_Bool _e_keyrouter_send_key_events_release(int type, Ecore_Event_Key *ev);
static Eina_Bool _e_keyrouter_send_key_event(int type, struct wl_resource *surface, struct wl_client *wc, Ecore_Event_Key *ev, Eina_Bool focused, unsigned int mode);

static Eina_Bool _e_keyrouter_send_key_events_focus(int type, struct wl_resource *surface, Ecore_Event_Key *ev, struct wl_resource **delivered_surface);
static void _e_keyrouter_event_generate_key(Ecore_Event_Key *ev, int type, struct wl_client *send_surface);

static Eina_Bool _e_keyrouter_is_key_grabbed(int key);
static Eina_Bool _e_keyrouter_check_top_visible_window(E_Client *ec_focus, int arr_idx);

static Eina_Bool
_e_keyrouter_is_key_grabbed(int key)
{
   if (!krt->HardKeys[key].keycode)
     {
        return EINA_FALSE;
     }
   if (krt->HardKeys[key].excl_ptr ||
        krt->HardKeys[key].or_excl_ptr ||
        krt->HardKeys[key].top_ptr ||
        krt->HardKeys[key].shared_ptr)
     {
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_e_keyrouter_event_key_free(void *data EINA_UNUSED, void *ev)
{
   Ecore_Event_Key *e = ev;

   eina_stringshare_del(e->keyname);
   eina_stringshare_del(e->key);
   eina_stringshare_del(e->string);
   eina_stringshare_del(e->compose);

   if (e->dev) ecore_device_unref(e->dev);

   E_FREE(e);
}

static void
_e_keyrouter_event_generate_key(Ecore_Event_Key *ev, int type, struct wl_client *send_surface)
{
   Ecore_Event_Key *ev_cpy = NULL;

   ev_cpy = E_NEW(Ecore_Event_Key, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev_cpy);

   KLDBG("Generate new key event! send to wl_surface: %p (pid: %d)", send_surface, e_keyrouter_util_get_pid(send_surface, NULL));

   ev_cpy->keyname = (char *)eina_stringshare_add(ev->keyname);
   ev_cpy->key = (char *)eina_stringshare_add(ev->key);
   ev_cpy->string = (char *)eina_stringshare_add(ev->string);
   ev_cpy->compose = (char *)eina_stringshare_add(ev->compose);

   ev_cpy->window = ev->window;
   ev_cpy->root_window = ev->root_window;
   ev_cpy->event_window = ev->event_window;

   ev_cpy->timestamp = (int)(ecore_time_get()*1000);
   ev_cpy->modifiers = ev->modifiers;

   ev_cpy->same_screen = ev->same_screen;
   ev_cpy->keycode = ev->keycode;

   ev_cpy->data = send_surface;
   ev_cpy->dev = ecore_device_ref(ev->dev);

   if (ECORE_EVENT_KEY_DOWN == type)
     ecore_event_add(ECORE_EVENT_KEY_DOWN, ev_cpy, _e_keyrouter_event_key_free, NULL);
   else
     ecore_event_add(ECORE_EVENT_KEY_UP, ev_cpy, _e_keyrouter_event_key_free, NULL);
}

/* Function for checking the existing grab for a key and sending key event(s) */
Eina_Bool
e_keyrouter_process_key_event(void *event, int type)
{
   Eina_Bool res = EINA_TRUE;
   Ecore_Event_Key *ev = event;
   struct wl_client *wc;

   if (!ev) goto finish;

   KLDBG("[%s] keyname: %s, key: %s, keycode: %d", (type == ECORE_EVENT_KEY_DOWN) ? "KEY_PRESS" : "KEY_RELEASE", ev->keyname, ev->key, ev->keycode);

   if (ev->data)
     {
        KLDBG("data is exist send to compositor: %p", ev->data);
        goto finish;
     }

   if (ev->modifiers != 0)
     {
        KLDBG("Modifier key delivered to Focus window : Key %s(%d)", ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keycode);
        goto finish;
     }

   if (krt->playback_daemon_surface)
     {
       wc = wl_resource_get_client(krt->playback_daemon_surface);
       if (wc)
         {
            _e_keyrouter_event_generate_key(ev, type, wc);
            KLDBG("Sent key to playback-daemon");
         }
     }

   if (krt->max_tizen_hwkeys < ev->keycode)
     {
        KLWRN("The key(%d) is too larger to process keyrouting: Invalid keycode", ev->keycode);
        goto finish;
     }

   if (!krt->HardKeys[ev->keycode].keycode) goto finish;

   if (!e_keyrouter_intercept_hook_call(E_KEYROUTER_INTERCEPT_HOOK_BEFORE_KEYROUTING, type, ev))
     {
        goto finish;
     }

   if ((ECORE_EVENT_KEY_UP == type) && (!krt->HardKeys[ev->keycode].press_ptr))
     {
        KLDBG("The release key(%d) isn't a processed by keyrouter!", ev->keycode);
        res = EINA_FALSE;
        goto finish;
     }

   //KLDBG("The key(%d) is going to be sent to the proper wl client(s) !", ev->keycode);
   KLDBG("[%s] keyname: %s, key: %s, keycode: %d", (type == ECORE_EVENT_KEY_DOWN) ? "KEY_PRESS" : "KEY_RELEASE", ev->keyname, ev->key, ev->keycode);
   if (_e_keyrouter_send_key_events(type, ev))
     res = EINA_FALSE;

finish:
   return res;
}

/* Function for sending key events to wl_client(s) */
static Eina_Bool
_e_keyrouter_send_key_events(int type, Ecore_Event_Key *ev)
{
   Eina_Bool res;
   if (ECORE_EVENT_KEY_DOWN == type)
     {
        res = _e_keyrouter_send_key_events_press(type, ev);
     }
  else
     {
        res = _e_keyrouter_send_key_events_release(type, ev);
     }
  return res;
}

static Eina_Bool
_e_keyrouter_send_key_events_release(int type, Ecore_Event_Key *ev)
{
   int pid = 0;
   char *pname = NULL, *cmd = NULL;
   E_Keyrouter_Key_List_NodePtr key_node_data;
   Eina_Bool res = EINA_TRUE, ret = EINA_TRUE;

   /* Deliver release  clean up pressed key list */
   EINA_LIST_FREE(krt->HardKeys[ev->keycode].press_ptr, key_node_data)
     {
        if (key_node_data->status == E_KRT_CSTAT_ALIVE)
          {
             res = _e_keyrouter_send_key_event(type, key_node_data->surface, key_node_data->wc, ev,
                                               key_node_data->focused, TIZEN_KEYROUTER_MODE_PRESSED);

             pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
             cmd = e_keyrouter_util_cmd_get_from_pid(pid);
             pname = e_keyrouter_util_process_name_get_from_cmd(cmd);
             KLINF("Release Pair : %s(%s:%d)(Focus: %d)(Status: %d) => wl_surface (%p) wl_client (%p) (pid: %d) (pname: %s)",
                      ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, key_node_data->focused,
                      key_node_data->status, key_node_data->surface, key_node_data->wc, pid, pname ?: "Unknown");
             if(pname) E_FREE(pname);
             if(cmd) E_FREE(cmd);
          }
        else
          {
             if (key_node_data->focused == EINA_TRUE)
               {
                  res = EINA_FALSE;
                  if (key_node_data->status == E_KRT_CSTAT_DEAD)
                    {
                       ev->data = key_node_data->wc;
                    }
                  else
                    {
                       ev->data = (void *)0x1;
                    }
               }
             KLINF("Release Pair : %s(%s:%d)(Focus: %d)(Status: %d) => wl_surface (%p) wl_client (%p) process is ungrabbed / dead",
                      ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, key_node_data->focused,
                      key_node_data->status, key_node_data->surface, key_node_data->wc);
          }

        E_FREE(key_node_data);
        if (res == EINA_FALSE) ret = EINA_FALSE;
     }
   krt->HardKeys[ev->keycode].press_ptr = NULL;

   return ret;
}

static Eina_Bool
_e_keyrouter_send_key_events_press(int type, Ecore_Event_Key *ev)
{
   unsigned int keycode = ev->keycode;
   struct wl_resource *surface_focus = NULL;
   E_Client *ec_focus = NULL;
   struct wl_resource *delivered_surface = NULL;
   Eina_Bool res = EINA_TRUE;
   int pid = 0;
   char *pname = NULL, *cmd = NULL;

   E_Keyrouter_Key_List_NodePtr key_node_data;
   Eina_List *l = NULL;

   ec_focus = e_client_focused_get();
   surface_focus = e_keyrouter_util_get_surface_from_eclient(ec_focus);

   if (krt->isPictureOffEnabled == 1)
     {
       EINA_LIST_FOREACH(krt->HardKeys[keycode].pic_off_ptr, l, key_node_data)
          {
            if (key_node_data)
                {
                 res = _e_keyrouter_send_key_event(type, key_node_data->surface, key_node_data->wc, ev, key_node_data->focused, TIZEN_KEYROUTER_MODE_SHARED);

                 pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
                 cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                 pname = e_keyrouter_util_process_name_get_from_cmd(cmd);
                 KLINF("PICTURE OFF : %s(%d) => wl_surface (%p) wl_client (%p) (pid: %d) (pname: %s)",
                       ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keycode, key_node_data->surface, key_node_data->wc, pid, pname ?: "Unknown");
                 if(pname) E_FREE(pname);
                 if(cmd) E_FREE(cmd);
                }
          }
       return res;
     }
   if (!_e_keyrouter_is_key_grabbed(ev->keycode))
     {
       res = _e_keyrouter_send_key_events_focus(type, surface_focus, ev, &delivered_surface);
       if (delivered_surface)
         {
            res = e_keyrouter_add_surface_destroy_listener(delivered_surface);
            if (res != TIZEN_KEYROUTER_ERROR_NONE)
              {
                 KLWRN("Failed to add surface to destroy listener (res: %d)", res);
              }
         }
       return res;
     }

   EINA_LIST_FOREACH(krt->HardKeys[keycode].excl_ptr, l, key_node_data)
     {
        if (key_node_data)
          {
             res = _e_keyrouter_send_key_event(type, key_node_data->surface, key_node_data->wc, ev,
                                               key_node_data->focused, TIZEN_KEYROUTER_MODE_EXCLUSIVE);

             pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
             cmd = e_keyrouter_util_cmd_get_from_pid(pid);
             pname = e_keyrouter_util_process_name_get_from_cmd(cmd);
             KLINF("EXCLUSIVE : %s(%s:%d) => wl_surface (%p) wl_client (%p) (pid: %d) (pname: %s)",
                      ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode,
                      key_node_data->surface, key_node_data->wc, pid, pname ?: "Unknown");
             if(pname) E_FREE(pname);
             if(cmd) E_FREE(cmd);
             return res;
          }
     }

   EINA_LIST_FOREACH(krt->HardKeys[keycode].or_excl_ptr, l, key_node_data)
     {
        if (key_node_data)
          {
             res = _e_keyrouter_send_key_event(type, key_node_data->surface, key_node_data->wc, ev,
                                               key_node_data->focused, TIZEN_KEYROUTER_MODE_OVERRIDABLE_EXCLUSIVE);

             pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
             cmd = e_keyrouter_util_cmd_get_from_pid(pid);
             pname = e_keyrouter_util_process_name_get_from_cmd(cmd);
             KLINF("OVERRIDABLE_EXCLUSIVE : %s(%s:%d) => wl_surface (%p) wl_client (%p) (pid: %d) (pname: %s)",
                     ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode,
                     key_node_data->surface, key_node_data->wc, pid, pname ?: "Unknown");
             if(pname) E_FREE(pname);
             if(cmd) E_FREE(cmd);

             return res;
          }
     }

   // Top position grab must need a focus surface.
   if (surface_focus)
     {
        EINA_LIST_FOREACH(krt->HardKeys[keycode].top_ptr, l, key_node_data)
          {
             if (key_node_data)
               {
                  if ((EINA_FALSE == krt->isWindowStackChanged) && (surface_focus == key_node_data->surface))
                    {
                       pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
                       cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                       pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

                       res = _e_keyrouter_send_key_event(type, key_node_data->surface, NULL, ev, key_node_data->focused,
                                                         TIZEN_KEYROUTER_MODE_TOPMOST);
                       KLINF("TOPMOST (TOP_POSITION) : %s (%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                                ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode,
                                key_node_data->surface, pid, pname ?: "Unknown");

                       if(pname) E_FREE(pname);
                       if(cmd) E_FREE(cmd);
                       return res;
                    }
                  krt->isWindowStackChanged = EINA_FALSE;

                  if (_e_keyrouter_check_top_visible_window(ec_focus, keycode))
                    {
                       E_Keyrouter_Key_List_NodePtr top_key_node_data = eina_list_data_get(krt->HardKeys[keycode].top_ptr);
                       pid = e_keyrouter_util_get_pid(top_key_node_data->wc, top_key_node_data->surface);
                       cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                       pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

                       res = _e_keyrouter_send_key_event(type, top_key_node_data->surface, NULL, ev, top_key_node_data->focused,
                                                         TIZEN_KEYROUTER_MODE_TOPMOST);
                       KLINF("TOPMOST (TOP_POSITION) : %s (%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                             ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode,
                             top_key_node_data->surface, pid, pname ?: "Unknown");

                       if(pname) E_FREE(pname);
                       if(cmd) E_FREE(cmd);
                       return res;
                    }
                  break;
               }
          }
       goto need_shared;
     }

   if (krt->HardKeys[keycode].shared_ptr)
     {
need_shared:
        res = _e_keyrouter_send_key_events_focus(type, surface_focus, ev, &delivered_surface);
        if (delivered_surface)
          {
             res = e_keyrouter_add_surface_destroy_listener(delivered_surface);
             if (res != TIZEN_KEYROUTER_ERROR_NONE)
               {
                  KLWRN("Failed to add wl_surface to destroy listener (res: %d)", res);
               }
          }
        EINA_LIST_FOREACH(krt->HardKeys[keycode].shared_ptr, l, key_node_data)
          {
             if (key_node_data)
               {
                  if (delivered_surface && key_node_data->surface == delivered_surface)
                    {
                       // Check for already delivered surface
                       // do not deliver double events in this case.
                       continue;
                    }
                  else
                    {
                       _e_keyrouter_send_key_event(type, key_node_data->surface, key_node_data->wc, ev, key_node_data->focused, TIZEN_KEYROUTER_MODE_SHARED);
                       pid = e_keyrouter_util_get_pid(key_node_data->wc, key_node_data->surface);
                       cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                       pname = e_keyrouter_util_process_name_get_from_cmd(cmd);
                       KLINF("SHARED : %s(%s:%d) => wl_surface (%p) wl_client (%p) (pid: %d) (pname: %s)",
                             ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, key_node_data->surface, key_node_data->wc, pid, pname ?: "Unknown");
                       if(pname) E_FREE(pname);
                       if(cmd) E_FREE(cmd);
                    }
               }
          }
        return res;
     }

   return EINA_FALSE;
}

static Eina_Bool
_e_keyrouter_send_key_events_focus(int type, struct wl_resource *surface_focus,  Ecore_Event_Key *ev, struct wl_resource **delivered_surface)
{
   E_Client *ec_top = NULL, *ec_focus = NULL;
   Eina_Bool below_focus = EINA_FALSE;
   struct wl_resource *surface = NULL;
   Eina_List* key_list = NULL;
   int *key_data = NULL;
   Eina_List *ll = NULL;
   int deliver_invisible = 0;
   Eina_Bool res = EINA_TRUE;
   int pid = 0;
   char *pname = NULL, *cmd = NULL;

   if (!e_keyrouter_intercept_hook_call(E_KEYROUTER_INTERCEPT_HOOK_DELIVER_FOCUS, type, ev))
     {
        if (ev->data && ev->data != (void *)0x1)
          {
             *delivered_surface = ev->data;
             ev->data = wl_resource_get_client(ev->data);
          }
        return res;
     }

   ec_top = e_client_top_get();
   ec_focus = e_client_focused_get();

   if (!krt->HardKeys[ev->keycode].registered_ptr && !e_keyrouter_is_registered_window(surface_focus) &&
         !IsNoneKeyRegisterWindow(surface_focus) && !krt->invisible_set_window_list)
     {
        pid = e_keyrouter_util_get_pid(NULL, surface_focus);
        cmd = e_keyrouter_util_cmd_get_from_pid(pid);
        pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

        res = _e_keyrouter_send_key_event(type, surface_focus, NULL,ev, EINA_TRUE, TIZEN_KEYROUTER_MODE_SHARED);
        KLINF("FOCUS DIRECT : %s(%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
               ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, surface_focus, pid, pname ?: "Unknown");
        *delivered_surface = surface_focus;
        if(pname) E_FREE(pname);
        if(cmd) E_FREE(cmd);
        return res;
     }

   // loop over to next window from top of window stack
   for (; ec_top != NULL; ec_top = e_client_below_get(ec_top))
     {
        surface = e_keyrouter_util_get_surface_from_eclient(ec_top);

        if(surface == NULL)
          {
             // Not a valid surface.
             continue;
          }
        if (ec_top->is_cursor) continue;

        // Check if window stack reaches to focus window
        if (ec_top == ec_focus)
          {
             KLINF("%p is focus e_client & wl_surface: focus %p ==> %p", ec_top, surface_focus, surface);
             below_focus = EINA_TRUE;
          }

        // Check for FORCE DELIVER to INVISIBLE WINDOW
        if (deliver_invisible && IsInvisibleGetWindow(surface))
          {
             pid = e_keyrouter_util_get_pid(NULL, surface);
             cmd = e_keyrouter_util_cmd_get_from_pid(pid);
             pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

             res = _e_keyrouter_send_key_event(type, surface, NULL, ev, EINA_TRUE, TIZEN_KEYROUTER_MODE_REGISTERED);
             KLINF("FORCE DELIVER : %s(%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                   ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode,
                   surface, pid, pname ?: "Unknown");
             *delivered_surface = surface;
             if(pname) E_FREE(pname);
             if(cmd) E_FREE(cmd);
             return res;
          }

        // Check for visible window first <Consider VISIBILITY>
        // return if not visible
        if (ec_top->visibility.obscured == E_VISIBILITY_FULLY_OBSCURED || ec_top->visibility.obscured == E_VISIBILITY_UNKNOWN)
          {
             continue;
          }

        // Set key Event Delivery for INVISIBLE WINDOW
        if (IsInvisibleSetWindow(surface))
          {
             deliver_invisible = 1;
          }

        if (IsNoneKeyRegisterWindow(surface))
          {
             // Registered None property is set for this surface
             // No event will be delivered to this surface.
             KLINF("wl_surface (%p) is a none register window.", surface);
             continue;
          }

        if (e_keyrouter_is_registered_window(surface))
          {
             // get the key list and deliver events if it has registered for that key
             // Write a function to get the key list for register window.
             key_list = _e_keyrouter_registered_window_key_list(surface);
             if (key_list)
               {
                  EINA_LIST_FOREACH(key_list, ll, key_data)
                    {
                       if(!key_data) continue;

                       if(*key_data == ev->keycode)
                         {
                            pid = e_keyrouter_util_get_pid(NULL, surface);
                            cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                            pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

                            res = _e_keyrouter_send_key_event(type, surface, NULL, ev, EINA_TRUE, TIZEN_KEYROUTER_MODE_REGISTERED);
                            KLINF("REGISTER : %s(%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                                  ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, surface, pid, pname ?: "Unknown");
                            *delivered_surface = surface;
                            if(pname) E_FREE(pname);
                            if(cmd) E_FREE(cmd);
                            return res;
                         }
                    }
               }
             else
               {
                  KLDBG("Key_list is Null for registered wl_surface %p", surface);
               }
          }

        if (surface != surface_focus)
          {
             if (below_focus == EINA_FALSE) continue;

             // Deliver to below Non Registered window
             else if (!e_keyrouter_is_registered_window(surface))
               {
                  pid = e_keyrouter_util_get_pid(NULL, surface);
                  cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                  pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

                  res = _e_keyrouter_send_key_event(type, surface, NULL, ev, EINA_TRUE, TIZEN_KEYROUTER_MODE_SHARED);
                  KLINF("NOT REGISTER : %s(%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                        ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, surface, pid, pname ?: "Unknown");
                  *delivered_surface = surface;
                  if(pname) E_FREE(pname);
                  if(cmd) E_FREE(cmd);
                  return res;
               }
             else continue;
          }
        else
          {
             // Deliver to Focus if Non Registered window
             if (!e_keyrouter_is_registered_window(surface))
               {
                  pid = e_keyrouter_util_get_pid(NULL, surface);
                  cmd = e_keyrouter_util_cmd_get_from_pid(pid);
                  pname = e_keyrouter_util_process_name_get_from_cmd(cmd);

                  res = _e_keyrouter_send_key_event(type, surface, NULL,ev, EINA_TRUE, TIZEN_KEYROUTER_MODE_SHARED);
                  KLINF("FOCUS : %s(%s:%d) => wl_surface (%p) (pid: %d) (pname: %s)",
                        ((ECORE_EVENT_KEY_DOWN == type) ? "Down" : "Up"), ev->keyname, ev->keycode, surface, pid, pname ?: "Unknown");
                  *delivered_surface = surface;
                  if(pname) E_FREE(pname);
                  if(cmd) E_FREE(cmd);
                  return res;
               }
             else continue;
          }
    }

    KLINF("Couldnt Deliver key:(%s:%d) to any window. Focused wl_surface: %p", ev->keyname, ev->keycode, surface_focus);
    return res;
}

static Eina_Bool
_e_keyrouter_check_top_visible_window(E_Client *ec_focus, int arr_idx)
{
   E_Client *ec_top = NULL;
   Eina_List *l = NULL, *l_next = NULL;
   E_Keyrouter_Key_List_NodePtr key_node_data = NULL;

   ec_top = e_client_top_get();

   while (ec_top)
     {
        if (!ec_top->visible && ec_top == ec_focus)
          {
             KLDBG("Top e_client (%p) is invisible(%d) but focus client", ec_top, ec_top->visible);
             return EINA_FALSE;
          }
        if (!ec_top->visible)
          {
             ec_top = e_client_below_get(ec_top);
             continue;
          }

        /* TODO: Check this client is located inside a display boundary */

        EINA_LIST_FOREACH_SAFE(krt->HardKeys[arr_idx].top_ptr, l, l_next, key_node_data)
          {
             if (key_node_data)
               {
                  if (ec_top == wl_resource_get_user_data(key_node_data->surface))
                    {
                       krt->HardKeys[arr_idx].top_ptr = eina_list_promote_list(krt->HardKeys[arr_idx].top_ptr, l);
                       KLDBG("Move a client(e_client: %p, wl_surface: %p) to first index of list(key: %d)",
                                ec_top, key_node_data->surface, arr_idx);
                       return EINA_TRUE;
                    }
               }
          }

        if (ec_top == ec_focus)
          {
             KLDBG("The e_client(%p) is a focus client", ec_top);
             return EINA_FALSE;
          }

        ec_top = e_client_below_get(ec_top);
     }
   return EINA_FALSE;
}

/* Function for sending key event to wl_client(s) */
static Eina_Bool
_e_keyrouter_send_key_event(int type, struct wl_resource *surface, struct wl_client *wc, Ecore_Event_Key *ev, Eina_Bool focused, unsigned int mode)
{
   struct wl_client *wc_send;

   if (surface == NULL)
     {
        wc_send = wc;
     }
   else
     {
        wc_send = wl_resource_get_client(surface);
     }

   if (!wc_send)
     {
        KLWRN("wl_surface: %p or wl_client: %p returns null wayland client", surface, wc);
        return EINA_FALSE;
     }

   if (ECORE_EVENT_KEY_DOWN == type)
     {
        if (mode == TIZEN_KEYROUTER_MODE_EXCLUSIVE ||
            mode == TIZEN_KEYROUTER_MODE_OVERRIDABLE_EXCLUSIVE ||
            mode == TIZEN_KEYROUTER_MODE_TOPMOST ||
            mode == TIZEN_KEYROUTER_MODE_REGISTERED)
          {
             focused = EINA_TRUE;
             ev->data = wc_send;
             KLDBG("Send only one key! wl_client: %p(%d)", wc_send, e_keyrouter_util_get_pid(wc_send, NULL));
          }
        else if (focused == EINA_TRUE)
          {
             ev->data = wc_send;
          }
        e_keyrouter_prepend_to_keylist(surface, wc, ev->keycode, TIZEN_KEYROUTER_MODE_PRESSED, focused);
     }
   else
     {
        if (focused == EINA_TRUE) ev->data = wc_send;
     }

   if (focused == EINA_TRUE) return EINA_FALSE;

   _e_keyrouter_event_generate_key(ev, type, wc_send);

   return EINA_TRUE;
}

struct wl_resource *
e_keyrouter_util_get_surface_from_eclient(E_Client *client)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL
     (client, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL
     (client->comp_data, NULL);

   return client->comp_data->wl_surface;
}

int
e_keyrouter_util_get_pid(struct wl_client *client, struct wl_resource *surface)
{
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   struct wl_client *cur_client = NULL;

   if (client) cur_client = client;
   else if (surface) cur_client = wl_resource_get_client(surface);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cur_client, 0);

   wl_client_get_credentials(cur_client, &pid, &uid, &gid);

   return pid;
}

char *
e_keyrouter_util_cmd_get_from_pid(int pid)
{
   Eina_List *l;
   E_Comp_Connected_Client_Info *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, NULL);

   EINA_LIST_FOREACH(e_comp->connected_clients, l, cdata)
     {
        if (cdata->pid == pid) return strdup(cdata->name);
     }

   return NULL;
}

typedef struct _keycode_map{
    xkb_keysym_t keysym;
    xkb_keycode_t keycode;
}keycode_map;

static void
find_keycode(struct xkb_keymap *keymap, xkb_keycode_t key, void *data)
{
   keycode_map *found_keycodes = (keycode_map *)data;
   xkb_keysym_t keysym = found_keycodes->keysym;
   int nsyms = 0;
   const xkb_keysym_t *syms_out = NULL;

   nsyms = xkb_keymap_key_get_syms_by_level(keymap, key, 0, 0, &syms_out);
   if (nsyms && syms_out)
     {
        if (*syms_out == keysym)
          {
             found_keycodes->keycode = key;
          }
     }
}

int
_e_keyrouter_keycode_get_from_keysym(struct xkb_keymap *keymap, xkb_keysym_t keysym)
{
   keycode_map found_keycodes = {0,};
   found_keycodes.keysym = keysym;
   xkb_keymap_key_for_each(keymap, find_keycode, &found_keycodes);

   return found_keycodes.keycode;
}

int
e_keyrouter_util_keycode_get_from_string(char * name)
{
   struct xkb_keymap *keymap = NULL;
   xkb_keysym_t keysym = 0x0;
   int keycode = 0;

   keymap = e_comp_wl->xkb.keymap;
   EINA_SAFETY_ON_NULL_GOTO(keymap, finish);

   keysym = xkb_keysym_from_name(name, XKB_KEYSYM_NO_FLAGS);
   EINA_SAFETY_ON_FALSE_GOTO(keysym != XKB_KEY_NoSymbol, finish);

   keycode = _e_keyrouter_keycode_get_from_keysym(keymap, keysym);

   KLDBG("request name: %s, return value: %d", name, keycode);

   return keycode;

finish:
   return 0;
}

char *
e_keyrouter_util_keyname_get_from_keycode(int keycode)
{
   struct xkb_state *state;
   xkb_keysym_t sym = XKB_KEY_NoSymbol;
   char name[256] = {0, };

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl->xkb.state, NULL);

   state = e_comp_wl->xkb.state;
   sym = xkb_state_key_get_one_sym(state, keycode);
   xkb_keysym_get_name(sym, name, sizeof(name));

   return strdup(name);
}

char *
e_keyrouter_util_process_name_get_from_cmd(char *cmd)
{
   int len, i;
   char pbuf = '\0';
   char *pname = NULL;
   if (cmd)
     {
        len = strlen(cmd);
        for (i = 0; i < len; i++)
          {
             pbuf = cmd[len - i - 1];
             if (pbuf == '/')
               {
                  pname = &cmd[len - i];
                  return strdup(pname);
               }
          }
     }
   return NULL;
}
#endif
