/**************************************************************************************************
 * WTalk
 *
 * Author: Razvan Madalin MATEI <matei.rm94@gmail.com>
 * Date last modified: July 2015
 *************************************************************************************************/

#include <string.h>   /* strlen, strdup */
#include <unistd.h>   /* read, write    */
#include <fcntl.h>    /* open           */
#include <ctype.h>    /* tolower        */
#include <sys/wait.h> /* waitpid        */
#include <jansson.h>  /* json_t handling */

#include "winternals/winternals.h" /* logs and errs */
#include "wxmpp/wxmpp.h"           /* xmpp stuff    */



#define BOARDTYPE_PATH "/etc/wyliodrin/boardtype" /* File path of the file named boardtype.
                                                     This file contains the name of the board.
                                                     Example: edison or arduinogalileo. */
#define SETTINGS_PATH  "/etc/wyliodrin/settings_" /* File path of "settings_<boardtype>.json".
                                                     <boardtype> is the string found in the file
                                                     named boardtype.
                                                     Example: settings_edison.json */



/* Variables to be used by all the modules */
const char *jid_str;        /* jid         */
const char *owner_str;      /* owner       */
const char *mount_file_str; /* mount file  */
const char *build_file_str; /* build file  */
const char *board_str;      /* board name  */

bool privacy = false;



extern void xmpp_connect(const char *jid, const char *pass); /* implemented in wxmpp.c */
extern json_t* file_to_json_t(const char *filename);         /* implemented in wjson.c */



/**
 * Get the string value of the key <key> in the json object <json>.
 *
 * Returns the associated value of string as a null terminated UTF-8 encoded string,
 * or NULL if there is not a key <key> in <json>, or there is a key <key>, but not of
 * string type.
 *
 * The retuned value is read-only and must not be modified or freed by the user.
 * It is valid as long as string exists, i.e. as long as its reference count has not dropped to 0.
 */
static const char *get_str_value(json_t *json, char *key) {
  json_t *value_json = json_object_get(json, key); /* value as JSON value */

  /* Sanity checks */
  if (value_json == NULL || !json_is_string(value_json)) {
    return NULL;
  } else {
    return json_string_value(value_json);
  }
}


/**
 * Read settings_<board_type> file, read wyliodrin.json, get jid, password and other data
 * needed by WTalk, connect to Wyliodrin XMPP server.
 *
 * Update /etc/resolv.conf if there is an entry named nameserver in wyliodrin.json.
 * Umount the mount file.
 * Configure wifi on edison boards.
 */
void wtalk() {
  int rc_int; /* Return code of integer type */
  int wifi_pid = -1; /* Pid of fork's child in which edison's wifi configuration is done */

  /* Get the type of board from the boardtype file */
  int boardtype_fd = open(BOARDTYPE_PATH, O_RDONLY); /* File descriptor of boardtype file */
  wsyserr(boardtype_fd == -1, "open");
  char boardtype[64]; /* Content of boardtype */
  memset(boardtype, 0, 64);
  rc_int = read(boardtype_fd, boardtype, 64);
  wsyserr(rc_int == -1, "read");

  /* Get the path of settings_<boardtype> file */
  char settings_path[128];
  rc_int = sprintf(settings_path, "%s%s.json", SETTINGS_PATH, boardtype);
  wsyserr(rc_int < 0, "sprintf");

  /* Get the content from the settings_<boardtype> file in a json_object */
  json_t *settings_json = file_to_json_t(settings_path); /* JSON object of settings_<boardtype> */
  wfatal(settings_json == NULL, "Invalid JSON is %s", settings_path);

  /* Get config_file value. This value contains the path to wyliodrin.json */
  const char *config_file_str = get_str_value(settings_json, "config_file"); /* config_file value */
  wfatal(config_file_str == NULL || strlen(config_file_str) == 0,
    "No non-empty config_file key of type string in %s", settings_path);

  /* Get the content from the wyliodrin.json file in a json object */
  json_t *config_json = file_to_json_t(config_file_str); /* config_file JSON */
  wfatal(config_json == NULL, "Invalid JSON in %s", config_file_str);

  /* Set privacy based on privacy value from wyliodrin.json (if exists) */
  json_t *privacy_json = json_object_get(config_json, "privacy");
  if (privacy_json != NULL && json_is_boolean(privacy_json) && json_is_true(privacy_json)) {
    privacy = true;
  }

  /* Get mountFile value. This value containts the path where the projects are to be mounted */
  mount_file_str = get_str_value(settings_json, "mountFile");
  wfatal(mount_file_str == NULL, "No non-empty mountFile key of type string in %s", settings_path);
  mount_file_str = strdup(mount_file_str);
  wfatal(mount_file_str == NULL, "strdup");

  /* Get buildFile value. This value containts the path where the projects are to be mounted */
  build_file_str = get_str_value(settings_json, "buildFile");
  wfatal(build_file_str == NULL, "No non-empty buildFile key of type string in %s", settings_path);
  build_file_str = strdup(build_file_str);
  wfatal(build_file_str == NULL, "strdup");

  /* Get the board value */
  board_str = get_str_value(settings_json, "board");
  wfatal(board_str == NULL, "No non-empty board key of type string in %s", settings_path);
  board_str = strdup(board_str);
  wfatal(board_str == NULL, "strdup");

  /* Get jid value from wyliodrin.json */
  jid_str = get_str_value(config_json, "jid");
  wfatal(jid_str == NULL, "No non-empty jid key of type string in %s", config_file_str);
  jid_str = strdup(jid_str);

  /* Get passwork value from wyliodrin.json */
  const char* password_str = get_str_value(config_json, "password");
  wfatal(password_str == NULL, "No non-empty password key of type string in %s", config_file_str);

  /* Get owner value from wyliodrin.json */
  owner_str = get_str_value(config_json, "owner");
  wfatal(owner_str == NULL, "No non-empty owner key of type string in %s", config_file_str);
  owner_str = strdup(owner_str);
  wfatal(owner_str == NULL, "strdup");

  /* Convert owner to lowercase */
  int i; /* browser */
  char *p = (char *)owner_str; /* remove const-ness of owner_str */
  for (i = 0; i < strlen(p); i++) {
    p[i] = tolower(p[i]);
  }

  /* Unmount the mountFile */
  if (strcmp(board_str, "server") != 0) {
    char umount_cmd[128];
    rc_int = sprintf(umount_cmd, "umount %s", mount_file_str);
    wsyserr(rc_int < 0, "sprintf");
    rc_int = system(umount_cmd);
    wsyserr(rc_int == -1, "system");
  }

  /* Configure wifi of Edison boards */
  if (strcmp(boardtype, "edison") == 0) {
    const char *ssid_str = get_str_value(config_json, "ssid");

    if (ssid_str != NULL && strlen(ssid_str) != 0) {
      const char *psk_str = get_str_value(config_json, "psk");
      if (psk_str != NULL) {
        /* Set wifi type: OPEN or WPA-PSK */
        char wifi_type[16];
        if (strlen(psk_str) == 0) {
          rc_int = sprintf(wifi_type, "OPEN");
        } else {
          rc_int = sprintf(wifi_type, "WPA-PSK");
        }
        wsyserr(rc_int < 0, "sprintf");

        /* Fork and exec configure_edison */
        wifi_pid = fork();
        wfatal(wifi_pid == -1, "fork");
        if (wifi_pid == 0) { /* Child */
          char *args[] = {"configure_edison", "--changeWiFi",
            wifi_type, (char *)ssid_str, (char *)psk_str, NULL};
          execvp(args[0], args);
          werr("configure_edison failed");
          exit(EXIT_FAILURE);
        }
      }
    }
  }

  /* Update /etc/resolv.conf if nameserver is a valid entry in wyliodrin.json */
  const char *nameserver_str = get_str_value(config_json, "nameserver");
  if (nameserver_str != NULL && strlen(nameserver_str) != 0) {
    int resolv_fd = open("/etc/resolv.conf", O_WRONLY | O_TRUNC);
    if (resolv_fd < 0) {
      werr("Could not open resolv.conf");
    } else {
      char to_write[128];
      rc_int = sprintf(to_write, "nameserver %s", nameserver_str);
      wsyserr(rc_int < 0, "sprintf");
      rc_int = write(resolv_fd, to_write, strlen(to_write));
      wsyserr(rc_int == -1, "write");
    }
  }

  /* Wait for wifi configuration */
  if (wifi_pid != -1) {
    waitpid(wifi_pid, NULL, 0);
  }

  /* Connect to XMPP server */
  xmpp_connect(jid_str, password_str);

  /* Cleaning */
  json_decref(config_json);
  json_decref(settings_json);
}

int main(int argc, char *argv[]) {
  wtalk();

  return 0;
}
