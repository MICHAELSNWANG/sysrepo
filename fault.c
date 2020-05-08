#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <sysrepo.h>
#include <sysrepo/plugins.h>
#include <sysrepo/values.h>

#include <sys/stat.h>
#include <time.h>

/* no synchronization is used in this example even though most of these
 * variables are shared between 2 threads, but the chances of encountering
 * problems is low enough to ignore them in this case */

/* session of our plugin, can be used until cleanup is called */
sr_session_ctx_t *sess;
/* structure holding all the subscriptions */
sr_subscription_ctx_t *subscription;
/* thread ID of the oven (thread) */
volatile pthread_t fm_tid;
volatile pthread_t basestation_tid;
volatile pthread_t Start_log_tid = NULL;
volatile pthread_t Watchdog_tid = NULL;
/* oven state value determining whether the food is inside the oven or not */
volatile int food_inside;
volatile int UE_inside;
/* oven state value determining whether the food is waiting for the oven to be ready */
volatile int insert_food_on_ready;
/* oven state value determining the current temperature of the oven */
//volatile unsigned int oven_temperature;
volatile unsigned int basestation_UE_number;
/* oven config value stored locally just so that it is not needed to ask sysrepo for it all the time */
//volatile unsigned int config_temperature;
volatile unsigned int config_ue_max;
volatile unsigned int enter_ue_number;
volatile int supervision_timer;

/****************************************************

function name: fm_thread

description:

parameters:

author: Michael Wang

*****************************************************/
static void *fm_thread(void *arg)
{
	sr_node_t *notif = NULL;
    sr_node_t *leaf = NULL;
	size_t notif_cnt = 0;
    int fault_id = 0;
    
    int rc;
    
    notif_cnt = 7;
    
    rc = sr_new_trees(notif_cnt, &notif);
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }

    rc = sr_node_set_name(&notif[0], "fault-id");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    //sr_node_set_str_data(&notif[0], SR_STRING_T, "fan source");
    //These are correct setting
    notif[0].type = SR_UINT16_T;
    notif[0].data.uint16_val = 10;
  
    rc = sr_node_set_name(&notif[1], "fault-source");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    //sr_node_set_str_data(&notif[0], SR_STRING_T, "fan source");
    notif[1].type = SR_STRING_T;
    notif[1].data.string_val = "fan source";
        
    rc = sr_node_set_name(&notif[2], "affected-objects");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    notif[2].type = SR_LIST_T;
        
    rc = sr_node_add_child(&notif[2], "name", NULL, &leaf);
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    leaf->type = SR_STRING_T;
    leaf->data.string_val = "fan object";
        
    rc = sr_node_set_name(&notif[3], "fault-severity");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    notif[3].type = SR_ENUM_T;
    notif[3].data.enum_val = "CRITICAL";
        
    rc = sr_node_set_name(&notif[4], "is-cleared");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    notif[4].type = SR_BOOL_T;
    notif[4].data.bool_val = "true";
        
    rc = sr_node_set_name(&notif[5], "fault-text");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    notif[5].type = SR_STRING_T;
    notif[5].data.string_val = "fan speed not enough";
        
    rc = sr_node_set_name(&notif[6], "event-time");
    if(SR_ERR_OK != rc) 
    {
        return rc;
    }
    notif[6].type = SR_STRING_T;
    notif[6].data.string_val = "2016-10-06T15:12:50-08:00";  
	
    while(1)//notif_cnt > 0)
    {  
      sleep(10);

      printf("Michael enter fm_thread: %d\n\n");
      
      scanf("%d", &fault_id);
      
      fflush(stdin);
      
      notif[0].data.uint16_val = fault_id;
      
      sr_event_notif_send_tree(sess, "/fault:alarm-notif", notif, notif_cnt, SR_EV_NOTIF_DEFAULT);      
    }

    sr_free_trees(notif, notif_cnt);	
}

/****************************************************

function name: fm_config_change_cb

description: 

parameters:

author: Michael Wang

*****************************************************/
static int
fm_config_change_cb(sr_session_ctx_t *session, const char *module_name, sr_notif_event_t event, void *private_ctx)
{
    int rc;
    sr_val_t *val;

    printf("Michael Enter fm CONFIG CHANGE CB! \n\n");
    fflush(stdout);

    rc = pthread_create((pthread_t *)&fm_tid, NULL, fm_thread, NULL);
    if (rc != 0) 
	{
      goto sys_error;
    }

    return SR_ERR_OK;

sr_error:
    SRP_LOG_ERR("Log: fm config change callback failed: %s.", sr_strerror(rc));
    return rc;

sys_error:
    sr_free_val(val);
    SRP_LOG_ERR("Log: fm config change callback failed: %s.", strerror(rc));
    return SR_ERR_OPERATION_FAILED;
}

/****************************************************

function name: fm_state_cb

description: 

parameters:

author: Michael Wang

*****************************************************/
static int
fm_state_cb(const char *xpath, sr_val_t **values, size_t *values_cnt,
        uint64_t request_id, const char *original_xpath, void *private_ctx)
{
    sr_val_t *vals;
    int rc;
    int values_count = 0;

    printf("Michael enter fm_state_cb \n\n");

    /* convenient functions such as this can be found in sysrepo/values.h */

    values_count = 7;

    rc = sr_new_values(values_count, &vals);
    if (SR_ERR_OK != rc) 
    {
        return rc;
    }

    sr_val_build_xpath(&vals[0], "/fault:active-alarm-list/active-alarms[fault-id='%d']", 10);
    vals[0].type = SR_LIST_T;
    //vals[0].data.uint16_val = 10;
    
    sr_val_build_xpath(&vals[1], "/fault:active-alarm-list/active-alarms[fault-id='%d']/fault-source", 10);
    vals[1].type = SR_STRING_T;
    vals[1].data.string_val = "fan source";

    sr_val_build_xpath(&vals[2], "/fault:active-alarm-list/active-alarms[fault-id='10']/affected-objects[name='%s']", "fan object");
    vals[2].type = SR_LIST_T;
    //vals[2].data.string_val = "fan object";

    sr_val_build_xpath(&vals[3], "/fault:active-alarm-list/active-alarms[fault-id='%d']/fault-severity", 10);
    vals[3].type = SR_ENUM_T;
    vals[3].data.enum_val = "CRITICAL";
    
    sr_val_build_xpath(&vals[4], "/fault:active-alarm-list/active-alarms[fault-id='%d']/is-cleared", 10);
    vals[4].type = SR_BOOL_T;
    vals[4].data.bool_val = "true";
    
    sr_val_build_xpath(&vals[5], "/fault:active-alarm-list/active-alarms[fault-id='%d']/fault-text", 10);
    vals[5].type = SR_STRING_T;
    vals[5].data.string_val = "fan speed not enough";
    
    sr_val_build_xpath(&vals[6], "/fault:active-alarm-list/active-alarms[fault-id='%d']/event-time", 10);
    vals[6].type = SR_STRING_T;
    vals[6].data.string_val = "2016-10-06T15:12:50-08:00";

    *values = vals;
    *values_cnt = values_count;

    return SR_ERR_OK;
}

/****************************************************

function name: sr_plugin_init_cb

description: 

parameters:

author: Michael Wang

*****************************************************/
int
sr_plugin_init_cb(sr_session_ctx_t *session, void **private_ctx)
{
    int rc;

    /* remember the session of our plugin, this is must doing */
    sess = session;
    
    printf("Michael enter fm sr_plugin_init_cb \n\n");

	rc = sr_module_change_subscribe(session, "fault", fm_config_change_cb, NULL, 0,
            SR_SUBSCR_EV_ENABLED | SR_SUBSCR_APPLY_ONLY, &subscription);
    if (rc != SR_ERR_OK) 
    {
        goto error;
    }
    
    printf("Michael leave fm sr_plugin_init_cb \n\n");

    /* subscribe as state data provider for the oven state data */
    rc = sr_dp_get_items_subscribe(session, "/fault:active-alarm-list", fm_state_cb, NULL, SR_SUBSCR_CTX_REUSE, &subscription);
    if (rc != SR_ERR_OK) 
    {
        goto error;
    }

    /* sysrepo/plugins.h provides an interface for logging */
    SRP_LOG_DBG_MSG("Log: fault plugin initialized successfully.");
    return SR_ERR_OK;

error:
    SRP_LOG_ERR("Log: fault plugin initialization failed: %s.", sr_strerror(rc));
    sr_unsubscribe(session, subscription);
    return rc;
}

/****************************************************

function name: sr_plugin_cleanup_cb

description: 

parameters:

author: Michael Wang

*****************************************************/
void
sr_plugin_cleanup_cb(sr_session_ctx_t *session, void *private_ctx)
{
    /* nothing to cleanup except freeing the subscriptions */
    sr_unsubscribe(session, subscription);
    SRP_LOG_DBG_MSG("Log: fault plugin cleanup finished.");
}
