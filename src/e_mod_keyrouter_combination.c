/*
 *   @file     e_mod_keyrouter_combination.c
 *   @brief    Implementation of e_mod_keyrouter_combination.c
 *   @author   Shubham Shrivastav shubham.sh@samsung.com
 *   @date     25th Feb 2016
 *   Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define E_COMP_WL
#include "e_mod_main_wl.h"
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include "ini.h"

#define MAX_LEN 64
#define DBUS_PATH "/com/burtonini/dbus/ping"
#define DBUS_IFACE "keyrouter.dbus.Signal"
#define DBUS_MSG_NAME "KEY_COMBINATION"
#define COMBINATION_TIME_OUT 4000
#define MAX_SUPPORTED_COMBINATION 255
#define DBUS_CONN_RETRY_TIMEOUT 0.2
#define DBUS_CONN_MAX_RETRY_COUNT 20
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

typedef unsigned long Time;
typedef struct _DbusConf
{
    char path[MAX_LEN];
    char interface[MAX_LEN];
    char msg[MAX_LEN];
} DbusConf;
typedef struct _KeyCombination
{
    DBusConnection * keyrouter_dbus_conn;
    DbusConf dbusconf;
    DBusError DBus_error;
    char combinationFilePath[MAX_LEN];
    GArray* _master_combinations;
    GArray* _current_matched_combinations;
    Time combination_timeout;
} KeyCombination;

static int keyCombinationInitialize = 0;
static int dbus_connection_init_retry_count = 0;
static Ecore_Timer *dbus_conn_timer = NULL;
KeyCombination g_key_combination;

static Eina_Bool _e_keyrouter_dbus_connection_init();
static int _e_keyrouter_search_key_combination(int keycode, Time timestamp);
static int _e_keyrouter_send_dbus_message(DBusConnection *bus, int Input);
static char * _e_keyrouter_substring(char *string, int position);
static int _e_keyrouter_parse_ini_config(void* user, const char* section, const char* name, const char* value);

static Eina_Bool
_e_keyrouter_dbus_connection_init()
{
   DBusError dBus_error;

   KLINF("_e_keyrouter_dbus_connection_init()");

   dbus_error_init(&dBus_error);
   g_key_combination.keyrouter_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dBus_error);
   if (!g_key_combination.keyrouter_dbus_conn)
     {
        KLWRN("[DBUS-CONNECTION-FAIL] DBUS connection is failed");
        if (dbus_error_is_set(&dBus_error))
          {
             KLWRN("[DBUS-ERROR] %s",dBus_error.message);
             dbus_error_free(&dBus_error);
          }
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
dbus_connection_init_retry_timercb()
{
   if (_e_keyrouter_dbus_connection_init())
     {
        keyCombinationInitialize = 1;
        dbus_conn_timer = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   else
     {
        if (dbus_connection_init_retry_count < DBUS_CONN_MAX_RETRY_COUNT)
          {
             dbus_connection_init_retry_count++;
             KLWRN("Failed to init dbus conn trying again, try count %d",dbus_connection_init_retry_count);
             return ECORE_CALLBACK_RENEW;
          }
        else
          {
             KLERR("Failed to init dbus connection for key combination after multiple tries");
             dbus_conn_timer = NULL;
             return ECORE_CALLBACK_CANCEL;
          }
     }
}

void
e_keyrouter_key_combination_init()
{
   memset(&g_key_combination, 0, sizeof(g_key_combination));
   g_key_combination.keyrouter_dbus_conn = NULL;
   dbus_conn_timer = NULL;
   snprintf(g_key_combination.dbusconf.path, strlen(DBUS_PATH)+1, DBUS_PATH);
   snprintf(g_key_combination.dbusconf.interface, strlen(DBUS_IFACE)+1, DBUS_IFACE);
   snprintf(g_key_combination.dbusconf.msg, strlen(DBUS_MSG_NAME)+1, DBUS_MSG_NAME);
   snprintf(g_key_combination.combinationFilePath, strlen(COMBINATION_FILE_PATH)+1, COMBINATION_FILE_PATH);

   g_key_combination._master_combinations = g_array_new(FALSE, FALSE, sizeof(GArray*));
   if (ini_parse((const char *) g_key_combination.combinationFilePath, _e_keyrouter_parse_ini_config, g_key_combination._master_combinations) < 0)
     {
        KLWRN("Can't load %s file", g_key_combination.combinationFilePath);
     }

   g_key_combination.combination_timeout = COMBINATION_TIME_OUT;
   if (_e_keyrouter_dbus_connection_init())
     {
        keyCombinationInitialize = 1;
     }
   else
     {
        dbus_conn_timer = ecore_timer_add(DBUS_CONN_RETRY_TIMEOUT, dbus_connection_init_retry_timercb, NULL);
        if (dbus_conn_timer != NULL)
          {
             KLWRN("Ecore Timer added for dbus init retry ");
          }
        else
          {
             KLERR("dbus connection init failed and unable to add ecore timer callback for init retry");
          }
     }
}

static char *
_e_keyrouter_substring(char *string, int position)
{
   char *pointer;
   int c;

   if (!string) return NULL;

   for (c = 0; c < position - 1; c++)
     string++;

   pointer = malloc(strlen(string) + 1);
   if (pointer == NULL)
     {
        KLWRN("Unable to allocate memory.");
        return NULL;
     }

   for (c = 0; *string != '\0'; c++)
     {
        *(pointer + c) = *string;
        string++;
     }
   *(pointer + c) = '\0';

   return pointer;
}

static int
_e_keyrouter_parse_ini_config(void* user, const char* section, const char* name, const char* value)
{
   int section_number, val;
   size_t needed;
   char *local_section, *c_num, *dup = NULL;
   GArray *masterArray, *childArray;

   if (!section) return -1;

   dup = strdup(section);
   c_num = _e_keyrouter_substring(dup, 12/*"Combination"*/);
   if (c_num == NULL)
     {
        KLWRN(" Unable to read config. substring is null.");
        return -1;
     }

   section_number = atoi(c_num);
   free(c_num);
   free(dup);
   if (section_number <= 0 || section_number > MAX_SUPPORTED_COMBINATION)
     {
        KLWRN("^[[36m Unable to read config. section_number is invalid. ^[[0m");
        return -1;
     }
   section_number--;

   masterArray = (GArray*) user;
   if (masterArray->len <= (unsigned int) section_number)
     {
        childArray = g_array_new(FALSE, FALSE, sizeof(int));
        g_array_insert_val(masterArray, section_number, childArray);
     }

   needed = snprintf(NULL, 0, "%s%d", "Combination", section_number + 1);
   local_section = malloc(needed + 1);
   if (local_section == NULL)
     {
        KLWRN("^[[36m Failed to allocate memory for local_section ^[[0m");
        return -1;
     }
   snprintf(local_section, needed + 1, "%s%d", "Combination", section_number + 1);

   if (MATCH(local_section, "1"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 0, val);
     }
   else if (MATCH(local_section, "2"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 1, val);
     }
   else if (MATCH(local_section, "3"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 2, val);
     }
   else if (MATCH(local_section, "4"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 3, val);
     }
   else if (MATCH(local_section, "5"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 4, val);
     }
   else if (MATCH(local_section, "6"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 5, val);
     }
   else if (MATCH(local_section, "7"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 6, val);
     }
   else if (MATCH(local_section, "8"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 7, val);
     }
   else if (MATCH(local_section, "9"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 8, val);
     }
   else if (MATCH(local_section, "10"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 9, val);
     }
   else if (MATCH(local_section, "11"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 10, val);
     }
   else if (MATCH(local_section, "12"))
     {
        val = atoi(value);
        childArray = g_array_index(masterArray,GArray*,section_number);
        g_array_insert_val(childArray, 11, val);
     }
   else
     {
        free(local_section);
        return 0; /* unknown section/name, error */
     }
    free(local_section);
    return 1;
}

void
e_keyrouter_process_key_combination(Time cur_time, int keycode, int state)
{
   int ret;

   if (!keyCombinationInitialize)
     {
        KLWRN("KeyCombinatioin support is not initiazlied yet");
        return ;
     }
   if (g_key_combination._master_combinations == NULL)
     {
        KLDBG(" Not searching key combination as Master combination is NULL");
     }
   if (state == ECORE_EVENT_KEY_UP)
     {
        ret = _e_keyrouter_search_key_combination(keycode, cur_time);

        if(ret > 0)
          {
             _e_keyrouter_send_dbus_message(g_key_combination.keyrouter_dbus_conn, ret);
          }
     }
}

static int
_e_keyrouter_send_dbus_message(DBusConnection *bus, int Input)
{
   DBusMessage *message = NULL;

   message = dbus_message_new_signal(g_key_combination.dbusconf.path, g_key_combination.dbusconf.interface, g_key_combination.dbusconf.msg);
   dbus_message_append_args(message, DBUS_TYPE_INT32, &Input, DBUS_TYPE_INVALID);

   if (!dbus_connection_send(bus, message, NULL))
     KLWRN( "DBUS sending MSG  FAILED!!");

   dbus_message_unref(message);
   return 1;
}

static int
_e_keyrouter_search_key_combination(int keycode, Time timestamp)
{
   static Time t = 0;
   unsigned int i, j = 0;
   int matchedIdx = 0, foundAt = 0;
   static int keyIdx = 0;
   static GArray *_source_Search_Array = NULL;
   GArray *childArray, *matched;

   if (timestamp - t >= g_key_combination.combination_timeout && keyIdx != 0)
     {
        t = timestamp;
        keyIdx = 0;
        g_array_free(_source_Search_Array, FALSE);

          {
             g_key_combination._current_matched_combinations = g_array_new(FALSE, FALSE, sizeof(GArray*));
             _source_Search_Array = g_key_combination._master_combinations;/*now update _current_matched_combinations assuming last key that invalidated as first key*/
             matchedIdx = 0;
             for (i = 0; i < _source_Search_Array->len; i++)
               {
                  GArray * childArray = g_array_index(_source_Search_Array,GArray*,i);
                  if (keycode == g_array_index(childArray,int,0))
                    {
                       g_array_insert_val(g_key_combination._current_matched_combinations, matchedIdx, childArray);
                       matchedIdx++;
                    }
               }

             if (g_key_combination._current_matched_combinations->len > 0)
               {
                  keyIdx = 1;/*assumed that first key is the last key that invalidated the combinaton.*/
                  _source_Search_Array = g_key_combination._current_matched_combinations;/*start next key combination matching from this assumed _current_matched_combinations*/
                  t = timestamp;
               }
             else /* the key that invalidated is unavailable in any master_combinations as first element*/
               {
                  keyIdx = 0;
                  t = timestamp;
                  g_array_free(g_key_combination._current_matched_combinations, FALSE);
               }
          }
        return -1;
     }

   g_key_combination._current_matched_combinations = g_array_new(FALSE, FALSE, sizeof(GArray*));
   if (keyIdx == 0) _source_Search_Array = g_key_combination._master_combinations;

   for (i = 0; i < _source_Search_Array->len; i++)
     {
        childArray = g_array_index(_source_Search_Array,GArray*,i);
        if (keycode == g_array_index(childArray,int,keyIdx))
          {
             g_array_insert_val(g_key_combination._current_matched_combinations, matchedIdx, childArray);
             matchedIdx++;
          }
     }

   if (keyIdx > 0)/* this needs to be freed for count > 0 as for keyIdx== 0 it will point to master_combinations!*/
     {
        g_array_free(_source_Search_Array, FALSE);/* this actually frees "last" current_matched_combinations*/
     }
   if (g_key_combination._current_matched_combinations->len < 1)/* the incoming key has invalidated the combination sequence*/
     {
        _source_Search_Array = g_key_combination._master_combinations;/*now update _current_matched_combinations assuming last key that invalidated as first key*/
        matchedIdx = 0;

        for (i = 0; i < _source_Search_Array->len; i++)
          {
             childArray = g_array_index(_source_Search_Array,GArray*,i);
             if (keycode == g_array_index(childArray,int,0))
               {
                  g_array_insert_val(g_key_combination._current_matched_combinations, matchedIdx, childArray);
                  matchedIdx++;
               }
          }

        if (g_key_combination._current_matched_combinations->len > 0)
          {
             keyIdx = 1;/*assumed that first key is the last key that invalidated the combinaton.*/
             _source_Search_Array = g_key_combination._current_matched_combinations;/*start next key combination matching from this assumed _current_matched_combinations*/
             t = timestamp;
          }
        else/* the key that invalidated is unavailable in any master_combinations as first element*/
          {
             keyIdx = 0;
             t = timestamp;
             g_array_free(g_key_combination._current_matched_combinations, FALSE);
          }
     }
   else
     {
        if (g_key_combination._current_matched_combinations->len == 1 && (unsigned int)(keyIdx+1) == g_array_index(g_key_combination._current_matched_combinations,GArray*,0)->len)
          {
             keyIdx = 0;
             t = timestamp;
             matched = g_array_index(g_key_combination._current_matched_combinations,GArray*,0);

             for (i = 0; i < matched->len; i++)
               KLDBG("[32m Matched Combination:|%d| [0m", g_array_index(matched,int,i));
             foundAt = 0;

             for (i = 0; i < g_key_combination._master_combinations->len; i++)
               {
                  childArray=g_array_index(g_key_combination._master_combinations,GArray*,i);
                  for (j = 0; j < childArray->len; j++)
                    {
                       if (childArray->len == matched->len && g_array_index(childArray,int,j) == g_array_index(matched,int,j))
                         {
                            foundAt = i + 1;
                         }
                       else
                         {
                            foundAt = FALSE;
                            break;
                         }
                    }
                  if (foundAt)
                    {
                       KLDBG("[32m COMBINATION FOUND AT : %d  [0m", foundAt);
                       break;
                    }
               }
             g_array_free(g_key_combination._current_matched_combinations, FALSE);/* we free this as combination is found and we need to start fresh for next key.*/
             return foundAt;
          }
        else/*continue search for next key*/
          {
             t = timestamp;
             keyIdx++;
             _source_Search_Array = g_key_combination._current_matched_combinations;
          }
     }

   return -1;
}
