/*--------------------------------
# Copyright (c) 2011 "Capensis" [http://www.capensis.com]
#
# This file is part of Canopsis.
#
# Canopsis is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Canopsis is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Canopsis.  If not, see <http://www.gnu.org/licenses/>.
# ---------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <stdbool.h>

#include <sys/time.h>

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_private.h>

#include "neb2amqp.h"
#include "cache.h"
#include "module.h"
#include "logger.h"

extern struct options g_options;

static int sockfd;

static bool amqp_errors = false;
static bool first = true;
static int amqp_lastconnect = 0;
static int amqp_wait_time = 10;

unsigned int amqp_connected = FALSE;

static amqp_connection_state_t conn = NULL;

void
on_error (int x, char const *context)
{

  if (x < 0)
    {
      char *errstr = amqp_error_string (-x);
      n2a_logger (LG_ERR, "AMQP: %s: %s\n", context, errstr);
      free (errstr);

      amqp_errors = true;
    }

}

void
on_amqp_error (amqp_rpc_reply_t x, char const *context)
{

  switch (x.reply_type)
    {
    case AMQP_RESPONSE_NORMAL:
      return;

    case AMQP_RESPONSE_NONE:
      n2a_logger (LG_ERR, "AMQP: %s: missing RPC reply type!\n", context);
      break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION: {
      char *err = amqp_error_string (x.library_error);
      n2a_logger (LG_ERR, "AMQP: %s: %s\n", context,
	      err);
      xfree (err);
      break;
    }

    case AMQP_RESPONSE_SERVER_EXCEPTION:
      switch (x.reply.id)
	{
	case AMQP_CONNECTION_CLOSE_METHOD:
	  {
	    amqp_connection_close_t *m =
	      (amqp_connection_close_t *) x.reply.decoded;
	    n2a_logger (LG_ERR,
		    "AMQP: %s: server connection error %d, message: %.*s\n",
		    context, m->reply_code, (int) m->reply_text.len,
		    (char *) m->reply_text.bytes);
	    break;
	  }
	case AMQP_CHANNEL_CLOSE_METHOD:
	  {
	    amqp_channel_close_t *m =
	      (amqp_channel_close_t *) x.reply.decoded;
	    n2a_logger (LG_ERR,
		    "AMQP: %s: server channel error %d, message: %.*s\n",
		    context, m->reply_code, (int) m->reply_text.len,
		    (char *) m->reply_text.bytes);
	    break;
	  }
	default:
	  n2a_logger (LG_ERR,
		  "AMQP: %s: unknown server error, method id 0x%08X\n",
		  context, x.reply.id);
	  break;
	}
      break;
    }

  amqp_errors = true;

}


void
amqp_connect (void)
{
  amqp_errors = false;

  struct timeval tv;
  gettimeofday (&tv, NULL);
  int now = tv.tv_sec;

  if ((amqp_lastconnect == 0) || (!amqp_connected && (now - amqp_lastconnect) >= amqp_wait_time) )
  {
    amqp_lastconnect = now;
    amqp_connected = FALSE;

    if (conn)
  	{
        amqp_destroy_connection(conn);
        amqp_socket_close(sockfd);
  	}
  	  
    n2a_logger (LG_DEBUG, "AMQP: Opening socket");
    on_error (sockfd = amqp_open_socket (g_options.hostname, g_options.port), "Opening socket");

    if (!amqp_errors)
  	{
        n2a_logger (LG_DEBUG, "AMQP: Init connection");
        conn = amqp_new_connection ();
  	}
  	
    if (!amqp_errors)
  	{
  	  amqp_set_sockfd (conn, sockfd);

  	  n2a_logger (LG_DEBUG, "AMQP: Logging");
  	  on_amqp_error (amqp_login(conn, g_options.virtual_host, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, g_options.userid, g_options.password), "Logging in");
  	}

    if (!amqp_errors)
  	{
  	  n2a_logger (LG_DEBUG, "AMQP: Open channel");
  	  amqp_channel_open (conn, 1);
  	  on_amqp_error (amqp_get_rpc_reply (conn), "Opening channel");
  	}

    if (!amqp_errors){
      n2a_logger (LG_INFO, "AMQP: Successfully connected");
      amqp_connected = TRUE;
    }
        
    if (amqp_connected)
  	{
      if (!first || g_options.purge) {
        unsigned int force = TRUE;
        n2a_pop_all_cache ((void *)&force);
      }
      first = false;
  	}
  }
}

void
amqp_disconnect (void)
{
  amqp_errors = false;
  
  if (amqp_connected)
    {
      n2a_logger (LG_DEBUG, "AMQP: Closing channel");
      on_amqp_error (amqp_channel_close (conn, 1, AMQP_REPLY_SUCCESS),
		     "Closing channel");

      n2a_logger (LG_DEBUG, "AMQP: Closing connection");
      on_amqp_error (amqp_connection_close (conn, AMQP_REPLY_SUCCESS),
		     "Closing connection");

      n2a_logger (LG_DEBUG, "AMQP: Ending connection");
      on_error (amqp_destroy_connection (conn), "Ending connection");
      
      conn = NULL;
      amqp_connected = FALSE;

      amqp_socket_close(sockfd);
      
      n2a_logger (LG_INFO, "AMQP: Successfully disconnected");
    }
  else
    {
      n2a_logger (LG_INFO, "AMQP: Impossible to disconnect, not connected");
    }
}

int
amqp_publish (const char *routingkey, const char *message)
{
  if (! amqp_connected)
    amqp_connect ();

  if (amqp_connected)
  {
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_CONTENT_ENCODING_FLAG;
    props.content_type = amqp_cstring_bytes ("application/json");
    props.content_encoding = amqp_cstring_bytes ("UTF-8");
    props.delivery_mode = 2;	/* persistent delivery mode */
    
   int result = amqp_basic_publish (conn,
			    1,
			    amqp_cstring_bytes (g_options.exchange_name),
			    amqp_cstring_bytes (routingkey),
			    0,
			    0,
			    &props,
			    amqp_cstring_bytes (message));

    on_error (result, "Publishing");

    if (amqp_errors)
    {
      n2a_record_cache (routingkey, message);
      n2a_logger (LG_INFO, "AMQP: Error on publish");
      amqp_disconnect ();
      return -1;
    }
    return 0;

  }else{
    n2a_record_cache (routingkey, message);
    return -1;
  }
}
