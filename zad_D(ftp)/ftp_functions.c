#include "ftp_functions.h"
#include "ftp_utils.h"
#include <limits.h>
#include <malloc.h>

const int BUF_SIZE = 255;

int ftp_acct(const int sock_fd) {
  send_status(220,
              "Permission already granted in response to PASS and/or USER");
}

int ftp_cwd(const int sock_fd, const char *params) {
  int temp = 0;
  if (*params != '\0') {
    if ((temp = change_path(arg, &path_buf, &path_buf_size,
                            &worker_data->dirinfo)) == 1) {
      strncpy(&worker_data->dirinfo.c_dir[worker_data->dirinfo.base_dir_len],
              &path_buf[worker_data->dirinfo.base_dir_len],
              worker_data->dirinfo.c_dir_size -
                  worker_data->dirinfo.base_dir_len - 1);
      ftp_sendline(sock_fd, "250 Okay");
    } else if (temp == -1) {
      ftp_send_ascii(sock_fd, "550 ");
      ftp_send_ascii(sock_fd, arg);
      ftp_sendline(sock_fd, " does not exist");
    } else {
      ftp_send_ascii(sock_fd, "550 ");
      ftp_send_ascii(sock_fd, arg);
      ftp_sendline(sock_fd, " is not a directory");
    }
  } else {
    send_status(504, "Client passed empty parameter");
    // ftp_sendline(sock_fd, "504 Client passed empty parameter");
  }
}

int ftp_help(const int sock_fd) {
  ftp_sendline(
      sock_fd,
      "214-The following commands are recognized.\r\n"
      " ACCT CWD CDUP DELE HELP LIST MKD MODE MDTM STOR PWD QUIT\r\n"
      " PASV RETR RNFR RNTO RMD SIZE SYST USER NOOP PORT SITE TYPE\r\n");
  send_status(214, "Help OK.");
}

int ftp_cdup(const int sock_fd, const char * restrict arg, char * restrict curr_path) {
  int cd_result = 0;

  if (*arg == '\0') {
    cd_result = safe_change_path(curr_path, "../");
    if (cd_result) {
      send_status(250, "Okay");
      return 1;
    } else if (cd_result == -1) {
      ftp_send_ascii(sock_fd, "550 ");
      ftp_send_ascii(sock_fd, arg);
      ftp_sendline(sock_fd, " does not exist");
      return 1;
    } else if (cd_result == -2) {
      ftp_send_ascii(sock_fd, "550 ");
      ftp_send_ascii(sock_fd, arg);
      ftp_sendline(sock_fd, " is not a directory");
      return 1;
    } else {
      return 0;
    }
    // if ((temp = change_path("../", &path_buf, &path_buf_size,
    //                         &worker_data->dirinfo)) == 1) {
    //   strncpy(&worker_data->dirinfo.c_dir[worker_data->dirinfo.base_dir_len],
    //           &path_buf[worker_data->dirinfo.base_dir_len],
    //           worker_data->dirinfo.c_dir_size -
    //               worker_data->dirinfo.base_dir_len - 1);
    // ftp_sendline(sock_fd, "250 Okay");
  } else {
    send_status(504, "Client passed non-empty parameter");
    return 1;
    // ftp_sendline(sock_fd, "504 Client passed non-empty parameter");
  }
}

int ftp_dele(const int sock_fd, const char *arg, const char *base_path,
             const char *rel_path) {
  int path_valid = 0;
  char path_buf[BUF_SIZE];

  if (*arg != '\0') {
    if (arg[0] == '/') {
      path_valid = path_valid(base_path, arg);
    } else {
      // TODO: concatenate to buffer
      path_valid = path_valid(base_path, path_buf);
    }
    // if (change_path(arg, &path_buf, &path_buf_size,
    //                 &worker_data->dirinfo) == 2) {
    if (path_valid) {
      if (unlink(arg) == 0) {
        ftp_send_ascii(sock_fd, "250 File \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\" was removed successfully.");
        return 1;
      } else if (errno == EISDIR) {
        ftp_send_ascii(sock_fd, "450 File \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\" is a directory.");
        return 1;
      } else {
        ftp_send_ascii(sock_fd, "450 Couldn't remove file \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\".");
        return 1;
      }
    } else {
      ftp_send_ascii(sock_fd, "550 File \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" does not exist.");
      return 1;
    }
  } else {
    send_status(504, "Client passed empty parameter");
    return 1;
    // ftp_sendline(sock_fd, "504 Client passed empty parameter");
  }
}

int ftp_list(const int sock_fd, const char *arg, int *conn_defined) {
  if (conn_defined) {
    if (change_path(cmd_argument, &path_buf, &path_buf_size,
                    &worker_data->dirinfo) == 1) {
      send_status(150, "Sending directory listing.");
      // ftp_sendline(sock_fd, "150 Sending directory listing.");
      pthread_mutex_lock(&worker_data->work_mtx);
      worker_data->data_op = LIST;
      pthread_cond_signal(&worker_data->work_ready);
      pthread_cond_wait(&worker_data->result_ready, &worker_data->work_mtx);
      if (worker_data->trans_ok) {
        send_status(226, "Directory send OK.");
        // ftp_sendline(sock_fd, "226 Directory send OK.");
      } else {
        send_status(425, "Transfer unsuccessfull.");
        // ftp_sendline(sock_fd, "425 Transfer unsuccessfull.");
      }
      pthread_mutex_unlock(&worker_data->work_mtx);
      *conn_defined = 0;
      return 1;
    } else {
      ftp_send_ascii(sock_fd, "550 Directory \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" doesn't exist or is a file.");
      return 0;
    }
  } else {
    ftp_sendline(sock_fd, "425 Use PORT or PASV first.");
    return 0;
  }
}

int ftp_mkd(const int sock_fd, const char *arg) {
  if (*arg != '\0') {
    change_path(arg, &path_buf, &path_buf_size, &worker_data->dirinfo);
    if (mkdir(path_buf, 0755) == 0) {
      ftp_send_ascii(sock_fd, "250 Directory \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" has been successfully created.");
    } else {
      ftp_send_ascii(sock_fd, "550 Directory \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" couldn't be created.");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter");
  }
}

int ftp_mode(const int sock_fd, const char *arg) {
  if (!strcmp(arg, "s")) {
    send_status(200, "Stream mode (deprecated)");
  } else {
    send_status(504, "Command not implemented for that parameter");
    // ftp_sendline(sock_fd, "504 Command not implemented for that
    // parameter");
  }
}

int ftp_mdtm(const int sock_fd, const char *arg) {
  struct tm lt;
  char timbuf[80];
  if (*arg != '\0') {
    if (change_path(arg, &path_buf, &path_buf_size, &worker_data->dirinfo) ==
        2) {
      stat(path_buf, &stat_buf);
      localtime_r(&stat_buf.st_mtime, &lt);
      strftime(timbuf, sizeof(timbuf), "%Y%m%d%H%M%S", &lt);
      ftp_send_ascii(sock_fd, "250 ");
      ftp_sendline(sock_fd, timbuf);
    } else {
      ftp_send_ascii(sock_fd, "550 File \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" does not exist.");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter.");
  }
}

int ftp_stor(const int sock_fd) {
  if (conn_defined) {
    if (*cmd_argument != '\0') {
      if (change_path(cmd_argument, &path_buf, &path_buf_size,
                      &worker_data->dirinfo) == 1) {
        ftp_send_ascii(sock_fd, "450 File \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\" is a directory.");
      } else {
        if ((temp_fjel = fopen(path_buf, "w")) != NULL) {
          ftp_sendline(sock_fd, "150 Ready to receive file");
          temp = 0;
          while (pthread_mutex_trylock(&worker_data->work_mtx) != 0 &&
                 temp < 3) {
            sleep(1);
            temp++;
          }
          if (temp < 3) {
            worker_data->file_w = temp_fjel;
            worker_data->data_op = STOR;
            pthread_cond_signal(&worker_data->work_ready);
            pthread_cond_wait(&worker_data->result_ready,
                              &worker_data->work_mtx);
            if (worker_data->trans_ok) {
              ftp_sendline(sock_fd, "226 File received OK.");
            } else {
              ftp_sendline(sock_fd, "451 Transfer unsuccessfull.");
            }
            conn_defined = 0;
            pthread_mutex_unlock(&worker_data->work_mtx);
          } else {
            ftp_sendline(sock_fd, "426 Connection timed out");
          }
        } else {
          ftp_send_ascii(sock_fd, "450 Cannot save file \"");
          ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
          ftp_sendline(sock_fd, "\".");
        }
      }
    } else {
      ftp_sendline(sock_fd, "504 Client passed empty parameter.");
    }
  } else {
    ftp_sendline(sock_fd, "425 Use PORT or PASV first.");
  }
  worker_data->start_pos = 0;
}

int ftp_pwd(const int sock_fd) {
  if (worker_data->dirinfo.c_dir[worker_data->dirinfo.base_dir_len] != '/') {
    ftp_sendline(sock_fd, "257 \"/\"");
  } else {
    ftp_send_ascii(sock_fd, "257 \"");
    ftp_send_ascii(
        sock_fd,
        &worker_data->dirinfo.c_dir[worker_data->dirinfo.base_dir_len]);
    ftp_send_ascii(sock_fd, "\"\r\n");
  }
}

int ftp_retr(const int sock_fd) {
  if (*cmd_argument != '\0') {
    temp = 0;
    while (pthread_mutex_trylock(&worker_data->work_mtx) != 0 && temp < 3) {
      sleep(1);
      temp++;
    }
    if (temp < 3) {
      if (worker_data->conn_defined) {
        if (change_path(cmd_argument, &path_buf, &path_buf_size,
                        &worker_data->dirinfo) == 2) {
          worker_data->fjel = path_buf;
          worker_data->data_op = RETR;
          pthread_cond_signal(&worker_data->work_ready);
          pthread_cond_wait(&worker_data->result_ready, &worker_data->work_mtx);
          if (worker_data->trans_ok) {
            ftp_sendline(sock_fd, "226 File send OK.");
          } else {
            ftp_sendline(sock_fd, "451 Transfer unsuccessfull.");
          }
          conn_defined = 0;
        } else {
          ftp_send_ascii(sock_fd, "550 File\"");
          ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
          ftp_sendline(sock_fd, "\" doesn't exist.");
        }
      } else {
        ftp_sendline(sock_fd, "425 Use PORT or PASV first.");
      }
      pthread_mutex_unlock(&worker_data->work_mtx);
    } else {
      ftp_sendline(sock_fd, "426 Connection timed out");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter.");
  }
  worker_data->start_pos = 0;
}

int ftp_rnfr(const int sock_fd) {
  if (*cmd_argument != '\0') {
    if (change_path(cmd_argument, &ren_buf, &ren_buf_size,
                    &worker_data->dirinfo) > 0) {
      ftp_sendline(sock_fd, "350 RNFR OK, waiting for RNTO.");
      rnfr_acc = 1;
    } else {
      ftp_send_ascii(sock_fd, "550 File\"");
      ftp_send_ascii(sock_fd, base_name_ptr(ren_buf));
      ftp_sendline(sock_fd, "\" doesn't exist.");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter.");
  }
}

int ftp_rnto(const int sock_fd) {
  if (*cmd_argument != '\0') {
    if (rnfr_acc) {
      if (change_path(cmd_argument, &path_buf, &path_buf_size,
                      &worker_data->dirinfo) > 0) {
        ftp_sendline(sock_fd, "550 File exists.");
      } else {
        if (rename(ren_buf, path_buf) == 0) {
          ftp_sendline(sock_fd, "250 File renamed succesfully");
        } else {
          ftp_sendline(sock_fd, "550 Failed to rename file.");
        }
      }
    } else {
      ftp_sendline(sock_fd, "550 You must use RNFR before using RNTO.");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter.");
  }
  rnfr_acc = 0;
}

int ftp_rmd(const int sock_fd) {
  if (*cmd_argument != '\0') {
    if (change_path(cmd_argument, &path_buf, &path_buf_size,
                    &worker_data->dirinfo) == 1) {
      if (rmdir(path_buf) == 0) {
        ftp_send_ascii(sock_fd, "250 Directory \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\" was removed successfully.");
      } else {
        ftp_send_ascii(sock_fd, "450 Couldn't remove directory \"");
        ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
        ftp_sendline(sock_fd, "\".");
      }
    } else {
      ftp_send_ascii(sock_fd, "550 Directory \"");
      ftp_send_ascii(sock_fd, base_name_ptr(path_buf));
      ftp_sendline(sock_fd, "\" does not exist.");
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter");
  }
}

int ftp_size(const int sock_fd, const char *arg, const char *path) {
  char buf[255 + 10 + 17];
  struct stat stat_buf;
  int bytes_written = 0;

  if (*arg != '\0') {
    // TODO: rewrite me pl0x
    if (change_path(arg, &path_buf, &path_buf_size, &worker_data->dirinfo) ==
        2) {
      stat(path_buf, &stat_buf);
      snprintf(buf, 5, "220 ");
      bytes_written =
          snprintf(buf[4], sizeof(buf) - 4 - 6 - 1, "%ld", stat_buf.st_size);
      bytes_written = snprintf(buf[bytes_written + 4],
                               sizeof(buf) - bytes_written - 4 - 1, " bytes");
      ftp_sendline(sock_fd, buf);
      return 1;
    } else {
      snprintf(buf, 11, "550 File \"");
      bytes_written =
          snprintf(buf, sizeof(buf) - 10 - 17 - 1, "%s", base_name_ptr(path));
      snprintf(buf, sizeof(buf) - bytes_written - 10 - 1, "\" does not exist.");
      ftp_sendline(sock_fd, buf);
      return 0;
    }
  } else {
    ftp_sendline(sock_fd, "504 Client passed empty parameter.");
    return 0;
  }
}
