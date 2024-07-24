#include <thread>
#include <string>
#include <iostream>

#include "mongoose.h"

struct thread_data {
  struct mg_mgr *mgr;
  unsigned long conn_id;  // Parent connection ID
  struct mg_str message;  // Original HTTP request
};

// static void *thread_function(void *param) {
static void* thread_function(void* param) {
  // struct thread_data *p = (struct thread_data *) param;
  thread_data* p = (thread_data*) param;
  sleep(2);                                 // Simulate long execution
  std::string* resp = new std::string {"hi"};
  mg_wakeup(p->mgr, p->conn_id, &resp, sizeof(resp));  // Respond to parent
  free((void *) p->message.buf);            // Free all resources that were
  // free(p);                               // passed to us
  delete p;                                 // passed to us
  return NULL;
}

// HTTP request callback
static void fn(struct mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    mg_http_message* hm = (mg_http_message*) ev_data;
    if(mg_match(hm->uri, mg_str("/"), NULL)) {
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "welcome\n");
    } else if(mg_match(hm->uri, mg_str("/admin"), NULL)) {
      mg_http_serve_opts opts = {
        .mime_types = "html=text/html"
      };
      mg_http_serve_file(c, hm, "admin.html", &opts);
    } else if(mg_match(hm->uri, mg_str("/admin-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
    } else if (mg_match(hm->uri, mg_str("/fast"), NULL)) {
      // Single-threaded code path, for performance comparison
      // The /fast URI responds immediately
      mg_http_reply(c, 200, "Host: foo.com\n", "hi\n");
    } else {
      MG_INFO(("Triggered multithreading, %.*s", hm->uri.len, hm->uri.buf));
      // Multithreading code path
      // thread_data* data = (thread_data*) calloc(1, sizeof(*data));  // Worker owns it
      thread_data* data = new thread_data;
      data->message = mg_strdup(hm->message);               // Pass message
      data->conn_id = c->id;
      data->mgr = c->mgr;
      // start_thread(thread_function, data);  // Start thread and pass data
      std::thread t(thread_function, data);
      t.detach();
    }
  } else if(ev == MG_EV_OPEN) {
  } else if(ev == MG_EV_WS_MSG) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    mg_ws_send(c, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
  } else if (ev == MG_EV_WAKEUP) {
    // struct mg_str *data = (struct mg_str *) ev_data;
    std::string* data = * (std::string**) ((mg_str*) ev_data)->buf;
    mg_http_reply(c, 200, "", "Result: %s\n", data->c_str());
    delete data;
  }
}

int main(void) {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);        // Initialise event manager
  mg_log_set(MG_LL_DEBUG);  // Set debug log level
  mg_http_listen(&mgr, "http://localhost:8000", fn, NULL);  // Create listener
  mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  for (;;) {             // Event loop
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);
  return 0;
}