/*
 * Copyright 2005,2008 WSO2, Inc. http://wso2.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "php.h"
#include "axis2_addr.h"
#include "wsf.h"
#include "wsf_util.h"
#include <axutil_error_default.h>
#include <axutil_log_default.h>
#include <axutil_uuid_gen.h>
#include <axiom_util.h>
#include <axis2_ctx.h>
#include "wsf_wsdl.h"
#include <axiom.h>
#include <php_main.h>
#include "wsf_client.h"
#include "zend_exceptions.h"

#include <axiom_soap_envelope.h>
#include <rampart_context.h>
#include <neethi_options.h>
#include <axis2_policy_include.h>
#include <neethi_engine.h>



void wsf_wsdl_do_request(
	zval *client_zval,
	zval* function_return_value,
	zval* return_value,
	zval *arguments,
	zval *classmap,
	axutil_env_t *env TSRMLS_DC);

axiom_soap_envelope_t* 
wsf_wsdl_create_request_envelope(
	char *php_payload,
	int soap_version,
	axutil_env_t *env TSRMLS_DC);

void 
wsf_wsdl_handle_client_security(
	HashTable *client_ht,
	zval **policy_array,
	axutil_env_t *env,
	axis2_svc_client_t * svc_client TSRMLS_DC);

void 
wsf_wsdl_handle_server_security(
	wsf_svc_info_t *svc,
	zval **policy_options,
	const axutil_env_t *env TSRMLS_DC);

void wsf_wsdl_create_dynamic_client(
	zval *this_ptr, 
	char *function, 
	int function_len,
	int arg_count, 
	zval *args, 
	zval *return_value,
	axutil_env_t * env TSRMLS_DC)
{
    zval *client_zval = NULL;
    zval **tmp = NULL;
    zval **wsdl_location = NULL;
    zval **end_point = NULL;
    zval **class_map = NULL;
    zval request_function, retval, param1, param2;
    zend_file_handle script;
    zval *params[2];
    zval *user_parameters = NULL;
    zval *function_parameters = NULL;
    php_stream *stream = NULL;
    FILE *new_fp = NULL;

    zval **service_name = NULL;
    zval **port_name = NULL;
    zval **use_mtom = NULL;

	zval *arguments = NULL;
	zval *classmap = NULL; 


    if (instanceof_function (Z_OBJCE_P (this_ptr), ws_client_proxy_class_entry TSRMLS_CC)) 
	{
        if (zend_hash_find (Z_OBJPROP_P (this_ptr), WSF_WSDL_WSCLIENT, sizeof (WSF_WSDL_WSCLIENT), 
                            (void **) & tmp) == SUCCESS) 
		{
            client_zval = *tmp;
        } else 
		{
            php_error_docref (NULL TSRMLS_CC, E_ERROR, " proxy created without wsclient");
            return;
        }
    } else if (instanceof_function (Z_OBJCE_P (this_ptr), ws_client_class_entry TSRMLS_CC)) 
	{
        client_zval = this_ptr;
    }
    if ( zend_hash_find ( Z_OBJPROP_P (client_zval), WSF_WSDL, sizeof (WSF_WSDL),
		(void **) &wsdl_location) == SUCCESS )
	{
		params[0] = &param1;
		params[1] = &param2;
		MAKE_STD_ZVAL(user_parameters);
		array_init(user_parameters);
		if(Z_TYPE_PP (wsdl_location) == IS_STRING)
		{
			add_assoc_string(user_parameters, WSF_WSDL,  Z_STRVAL_PP(wsdl_location), 1);
		}else if(Z_TYPE_PP(wsdl_location) == IS_ARRAY)
		{
			zval **tmpval = NULL;
			HashTable *ht = Z_ARRVAL_PP(wsdl_location);
			if(zend_hash_find(ht, WSF_WSDL, sizeof(WSF_WSDL), (void **)&tmpval) == SUCCESS &&
				Z_TYPE_PP(tmpval) == IS_STRING)
			{
				add_assoc_string(user_parameters, WSF_WSDL, Z_STRVAL_PP(tmpval), 1);
			}else
			{
				AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, "WSDL file name not specified ");
				return;
			}
			if(zend_hash_find(ht, WSF_HTTP_AUTH_USERNAME, sizeof(WSF_HTTP_AUTH_USERNAME), (void **)&tmpval) == SUCCESS &&
				Z_TYPE_PP(tmpval) == IS_STRING)
			{
				add_assoc_string(user_parameters, WSF_HTTP_AUTH_USERNAME, Z_STRVAL_PP(tmpval), 1);
			}
			if(zend_hash_find(ht, WSF_HTTP_AUTH_PASSWORD, sizeof(WSF_HTTP_AUTH_PASSWORD), (void **)&tmpval) == SUCCESS &&
				Z_TYPE_PP(tmpval) == IS_STRING)
			{
				add_assoc_string(user_parameters, WSF_HTTP_AUTH_PASSWORD, Z_STRVAL_PP(tmpval), 1);
			}
			if(zend_hash_find(ht, WSF_HTTP_AUTH_TYPE, sizeof(WSF_HTTP_AUTH_TYPE), (void **)&tmpval) == SUCCESS &&
				Z_TYPE_PP(tmpval) == IS_STRING)
			{
				add_assoc_string(user_parameters, WSF_HTTP_AUTH_TYPE, Z_STRVAL_PP(tmpval), 1);
			}
		}
	}else 
	{
		return;    
	}
    if ( zend_hash_find ( Z_OBJPROP_P (client_zval), WSF_TO, sizeof (WSF_TO),
		(void **) &end_point) == SUCCESS)
	{
		if(Z_TYPE_PP(end_point) == IS_STRING)
		{
			add_assoc_string(user_parameters, WSF_WSDL_ENDPOINT,  Z_STRVAL_PP(end_point), 1);
		}else if(Z_TYPE_PP(end_point) == IS_ARRAY)
		{	
			zval **tmpval = NULL;
			HashTable *ht = Z_ARRVAL_PP(end_point);
			if(zend_hash_find(ht, WSF_WSA_ADDRESS, sizeof(WSF_WSA_ADDRESS), 
				(void **)&tmpval) == SUCCESS && Z_TYPE_PP(tmpval) == IS_STRING)
			{
				add_assoc_string(user_parameters, WSF_WSDL_ENDPOINT, Z_STRVAL_PP(end_point), 1);
			}
		}

    }
    if ( zend_hash_find ( Z_OBJPROP_P (client_zval), WSF_WSDL_CLASSMAP,  sizeof (WSF_WSDL_CLASSMAP),
		(void **) &class_map) == SUCCESS && Z_TYPE_PP (class_map) == IS_ARRAY)
	{
            add_assoc_zval(user_parameters, WSF_WSDL_CLASSMAP, *class_map);
            classmap = *class_map;
            arguments = args;
    }

    if ( zend_hash_find ( Z_OBJPROP_P (this_ptr), WSF_SERVICE_NAME, sizeof (WSF_SERVICE_NAME),
		(void **) &service_name) == SUCCESS && Z_TYPE_PP (service_name) == IS_STRING)
	{
            add_assoc_string(user_parameters, WSF_SERVICE_NAME, Z_STRVAL_PP(service_name), 1);
    }

    if (zend_hash_find(Z_OBJPROP_P (this_ptr), WSF_PORT_NAME, sizeof (WSF_PORT_NAME),
        (void **) &port_name) == SUCCESS && Z_TYPE_PP (port_name) == IS_STRING)
	{
        add_assoc_string(user_parameters, WSF_PORT_NAME, Z_STRVAL_PP(port_name), 1);
    }

    if (client_zval && zend_hash_find ( Z_OBJPROP_P (client_zval), WSF_USE_MTOM, 
		sizeof (WSF_USE_MTOM), (void **) &use_mtom) == SUCCESS)
	{
            add_assoc_zval(user_parameters, WSF_USE_MTOM, *use_mtom);
    }
        
    MAKE_STD_ZVAL(function_parameters);
    array_init(function_parameters);
    add_assoc_string(function_parameters, WSF_WSDL_INVOKE_FUNCTION,  function, 1);
    add_assoc_long(function_parameters, WSF_WSDL_ARG_COUNT, arg_count);
    add_assoc_zval(function_parameters, WSF_WSDL_ARG_ARRAY, args);
    
    ZVAL_STRING(&request_function, WSF_WSDL_REQ_FUNCTION, 0);
    ZVAL_ZVAL(params[0], user_parameters, NULL, NULL);
    INIT_PZVAL(params[0]);
    ZVAL_ZVAL(params[1], function_parameters, NULL, NULL);
    INIT_PZVAL(params[1]);

	script.type = ZEND_HANDLE_FP;
	script.filename = WSF_WSDL_DYNAMIC_INVOC_SCRIPT;
	script.opened_path = NULL;
	script.free_filename = 0;


	stream  = php_stream_open_wrapper(WSF_WSDL_DYNAMIC_INVOC_SCRIPT, "rb", USE_PATH|REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);
	if(!stream)
		return;

	if (php_stream_cast(stream, PHP_STREAM_AS_STDIO|PHP_STREAM_CAST_RELEASE, (void*)&new_fp, REPORT_ERRORS) == FAILURE)    {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, 
                     "[wsf_wsdl] Unable to open script file or file not found");
	}
	script.handle.fp =  new_fp;
	if(script.handle.fp)
	{
		int lint_script = 1;
		{
			zval check_function, retval1;
			ZVAL_STRING(&check_function, WSF_WSDL_CHECK_FUNCTION, 0);
			if (call_user_function (EG (function_table), (zval **) NULL, 
				&check_function, &retval1, 0,NULL TSRMLS_CC) == SUCCESS)
			{
					if(Z_TYPE(retval1) == IS_LONG && Z_LVAL(retval1) == 1)
					{
						lint_script = 0;							
					}
			}
		}
		if(lint_script)
		{
			php_lint_script (&script TSRMLS_CC); 
		}
	}
	if (call_user_function (EG (function_table), (zval **) NULL, &request_function, &retval, 2,
                params TSRMLS_CC) == SUCCESS )
	{
        if (Z_TYPE(retval) == IS_ARRAY && Z_ARRVAL (retval) != IS_NULL)
		{
			wsf_wsdl_do_request(client_zval, &retval, return_value, arguments,classmap, env TSRMLS_CC);

		}else if (Z_TYPE_P(&retval) == IS_STRING)
		{
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Error occurred in script: %s",Z_STRVAL_P(&retval));
        }
    }
}


void 
wsf_wsdl_do_request(zval *client_zval, 
					zval *function_return_value,
                    zval *return_value,
					zval *arguments, 
					zval *classmap, 
					axutil_env_t *env TSRMLS_DC)
{
    axis2_svc_client_t *svc_client = NULL;
    axis2_options_t *client_options = NULL;
    ws_object_ptr intern = NULL;
    axiom_soap_envelope_t *response_envelope = NULL;
    char *php_payload = NULL;
    char *endpoint_address = NULL;
    axis2_endpoint_ref_t *to_epr = NULL;
    char *soap_action = NULL;
    char *wsa_action = NULL;
    int soap_version = 2; 
    zval **policy_options = NULL;
    zval **input_headers = NULL;
    char *response_sig_model_string = NULL;
    char *wsdl_dom_string = NULL;
	int rest_enabled = AXIS2_FALSE;

    axiom_node_t *res_payload = NULL;

    int has_fault = AXIS2_FALSE;
    axis2_char_t *res_text = NULL;
    axiom_soap_body_t *soap_body = NULL;
    axiom_node_t *body_base_node = NULL;
    axiom_soap_header_t *soap_header = NULL;
    axiom_node_t *header_base_node = NULL;
    axiom_soap_fault_t *soap_fault = NULL;
    axiom_node_t *fault_node = NULL;
    zval *rfault;
    axiom_node_t *request_node = NULL;

    
    zval response_function, *res_retval, res_param1, res_param2, res_param3, 
		res_param4, res_param5;
    zval *res_params[6];
    zval *response_parameters;

    HashTable *ht_return = Z_ARRVAL_P(function_return_value);
    zval **tmp_options = NULL;
    axis2_char_t *proxy_host = NULL;
    axis2_char_t *proxy_port = NULL;
    
    if(zend_hash_find(ht_return, WSF_WSDL_ENDPOINT_URI, sizeof(WSF_WSDL_ENDPOINT_URI),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_STRING )
	{
        endpoint_address = Z_STRVAL_PP(tmp_options);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "Endpoint address is :- %s",
                         endpoint_address);
    }
    if(zend_hash_find(ht_return, WSF_WSDL_BINDING_DETAILS, sizeof(WSF_WSDL_BINDING_DETAILS),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_ARRAY)
	{
        HashTable *ht_binding = Z_ARRVAL_PP(tmp_options);
        zval **binding_options = NULL;
        
        if(zend_hash_find(ht_binding, WSF_WSDL_WSA, sizeof(WSF_WSDL_WSA), 
			(void **)&binding_options) == SUCCESS && Z_TYPE_PP(binding_options) == IS_STRING)
		{
            wsa_action = Z_STRVAL_PP(binding_options);
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, 
				WSF_PHP_LOG_PREFIX "WSA action is :- %s", wsa_action);
        }
        if(zend_hash_find(ht_binding, WSF_WSDL_SOAP, sizeof(WSF_WSDL_SOAP), 
			(void **)&binding_options) == SUCCESS && Z_TYPE_PP(binding_options) == IS_STRING)
		{
            soap_action = Z_STRVAL_PP(binding_options);
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, 
				WSF_PHP_LOG_PREFIX "soap action is :- %s", soap_action);
        }
        if(zend_hash_find(ht_binding, WSF_WSDL_SOAP_VERSION, sizeof(WSF_WSDL_SOAP_VERSION), 
			(void **)&binding_options) == SUCCESS && Z_TYPE_PP(binding_options) == IS_LONG)
		{
            soap_version = Z_LVAL_PP(binding_options);
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
				"soap version is :- %d",soap_version);
        }
    }

    
    if(zend_hash_find(ht_return, WSF_WSDL_REQ_PAYLOAD, sizeof(WSF_WSDL_REQ_PAYLOAD),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_STRING )
	{
         php_payload = Z_STRVAL_PP(tmp_options);
         AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
                          "request payload :- %s", php_payload);
    }
    
    if(zend_hash_find(ht_return, WSF_WSDL_POLICY_NODE, sizeof(WSF_WSDL_POLICY_NODE),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_ARRAY)
	{
        policy_options = tmp_options;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "policy array found");
    }
    
    if(zend_hash_find(ht_return, WSF_WSDL_RES_SIG_MODEL, sizeof(WSF_WSDL_RES_SIG_MODEL),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_STRING )
	{
        response_sig_model_string = Z_STRVAL_PP(tmp_options);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
			"response sig model :- %s", response_sig_model_string);
    }
    
    if(zend_hash_find(ht_return, WSF_WSDL_DOM, sizeof(WSF_WSDL_DOM),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_STRING )
	{
        wsdl_dom_string = Z_STRVAL_PP(tmp_options);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "WSDL DOM string found");
    }

    /* retrieve the header information and set them on service client */
    
    if(zend_hash_find(ht_return, WSF_WSDL_INPUT_HEADERS, sizeof(WSF_WSDL_INPUT_HEADERS),
		(void **)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_ARRAY )
	{
        input_headers = tmp_options;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "input headers found");
    }
    
    WSF_GET_OBJ (svc_client, client_zval, axis2_svc_client_t, intern);
    client_options = (axis2_options_t *) axis2_svc_client_get_options (svc_client, env);


    if(input_headers)
	{

        HashPosition pos;
        zval **param = NULL;
        char *header_str = NULL;
        axiom_node_t *header_node = NULL;
        
        for (zend_hash_internal_pointer_reset_ex (Z_ARRVAL_PP(input_headers), &pos);
                zend_hash_get_current_data_ex (Z_ARRVAL_PP(input_headers), 
				(void **) &param, &pos) == SUCCESS;
                zend_hash_move_forward_ex (Z_ARRVAL_PP(input_headers), &pos)) 
		{
            
            if(Z_TYPE_PP(param) == IS_STRING) 
			{
                header_str = Z_STRVAL_PP(param);
                header_node = wsf_util_deserialize_buffer(env, header_str);
                axis2_svc_client_add_header(svc_client, env, header_node);
            }
        } 
    }

	/** reset xml parser to stop libxml2 crash */    
    axis2_options_set_xml_parser_reset (client_options, env, AXIS2_FALSE);
    
	wsf_client_set_options(Z_OBJPROP_P (client_zval), NULL,
                           env, client_options, svc_client, &rest_enabled TSRMLS_CC);
    
    wsf_client_enable_ssl (Z_OBJPROP_P (client_zval), env, client_options TSRMLS_CC);

    /** set http authentication parameters */
    wsf_client_set_http_auth_info(Z_OBJPROP_P (client_zval), env, client_options TSRMLS_CC);

    /** set proxy authentication parameters */
    wsf_client_set_proxy_auth_info(Z_OBJPROP_P (client_zval), env, client_options TSRMLS_CC);
    
    if(policy_options)
	{
        wsf_wsdl_handle_client_security(Z_OBJPROP_P (client_zval), 
                                        policy_options, env, svc_client TSRMLS_CC);
	}

    if(zend_hash_find ( Z_OBJPROP_P (client_zval), WSF_TO, sizeof (WSF_TO),
		(void **) &tmp_options) == SUCCESS && Z_TYPE_PP (tmp_options) == IS_STRING)
	{
        endpoint_address = Z_STRVAL_PP (tmp_options);
	}
	
	to_epr = axis2_endpoint_ref_create (env, endpoint_address);
    axis2_options_set_to (client_options, env, to_epr);
	
    /** add proxy options **/

    if (zend_hash_find (Z_OBJPROP_P (client_zval), WSF_PROXY_HOST, sizeof (WSF_PROXY_HOST),
                        (void **)&tmp_options) == SUCCESS) 
	{
        proxy_host = Z_STRVAL_PP (tmp_options);
    }
    if (zend_hash_find (Z_OBJPROP_P (client_zval), WSF_PROXY_PORT, sizeof (WSF_PROXY_PORT),
                        (void **)&tmp_options) == SUCCESS) 
	{
        proxy_port = Z_STRVAL_PP (tmp_options);
    }
    if (proxy_host && proxy_port) {
        axis2_svc_client_set_proxy (svc_client, env, proxy_host, proxy_port);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,WSF_PHP_LOG_PREFIX \
         "setting proxy options %s -- %s -- ", proxy_host, proxy_port);
    }

    if(soap_action)
	{
        axutil_string_t *action_string = axutil_string_create (env, soap_action);
        axis2_options_set_soap_action (client_options, env, action_string);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
        "soap action present :- %s", soap_action);
    }
    
    if(wsa_action)
	{
        axis2_options_set_action(client_options, env, wsa_action);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
                         "[wsf_wsdl] addressing action present :- %s",
                         wsa_action);
        axis2_svc_client_engage_module (svc_client, env, WSF_MODULE_ADDRESSING);
    }
    
    
    if (zend_hash_find (Z_OBJPROP_P (client_zval), WSF_USE_SOAP, sizeof (WSF_USE_SOAP),
                        (void **)&tmp_options) == SUCCESS) 
	{
		if (Z_TYPE_PP (tmp_options) == IS_STRING) 
		{
			char *value = NULL;
			value = Z_STRVAL_PP (tmp_options);
			if (value && strcmp (value, WSF_SOAP_VERSION_1_2) == 0) 
			{
				soap_version = 2;
				AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "soap version SOAP12");
            } else if (value && strcmp (value, WSF_SOAP_VERSION_1_1) == 0) 
			{
              soap_version = 1;
              AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,WSF_PHP_LOG_PREFIX "soap version SOAP11");
            }
        } else if (Z_TYPE_PP (tmp_options) == IS_DOUBLE) 
		{
            double val = Z_DVAL_PP (tmp_options);
            if (val == 1.2) 
			{
              soap_version = 2;
            } else if (val == 1.1) 
			{
              soap_version = 1;
            }
        }
    }
    if (soap_version)
	{
      axis2_options_set_soap_version (client_options, env, soap_version);
      AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX " soap version in wsdl mode is %d",
		       soap_version);
    }
    
    if(!php_payload)
	{
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX " request payload is not found");
       /* return; */
    }
   
    request_node = wsf_util_deserialize_buffer(env, php_payload);
    
    if(!request_node) 
	{
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX \
                         " Error in creating request payload dom");
        return;
    }
    
    if(zend_hash_find(ht_return, WSF_WSDL_ATTACHMENT_MAP, sizeof(WSF_WSDL_ATTACHMENT_MAP),
		(void**)&tmp_options) == SUCCESS && Z_TYPE_PP(tmp_options) == IS_ARRAY) 
	{
        int enable_mtom = AXIS2_TRUE;
        int enable_swa = AXIS2_FALSE;
        HashTable *client_ht = NULL;
        HashTable *ht_attachments = Z_ARRVAL_PP(tmp_options);

        client_ht = Z_OBJPROP_P (client_zval);

        if (client_ht && zend_hash_find (client_ht, WSF_USE_MTOM, sizeof (WSF_USE_MTOM),
                (void **) &tmp_options) == SUCCESS) 
		{
            if (Z_TYPE_PP (tmp_options) == IS_BOOL && Z_BVAL_PP (tmp_options) == 0) 
			{
                enable_mtom = AXIS2_FALSE;
            } 
			else if (Z_TYPE_PP (tmp_options) == IS_STRING) 
			{
                char *value = NULL;
                value = Z_STRVAL_PP (tmp_options);
                /* Check if SOAP with Attachments (SwA) has been enabled */
                if (value && (stricmp (value, WSF_SWA) == 0) )
				{

                    AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, WSF_PHP_LOG_PREFIX "SwA enabled");
                    enable_swa = AXIS2_TRUE;
                } 
            }
        }


        axis2_options_set_enable_mtom (client_options, env, enable_mtom);
        wsf_util_set_attachments_with_cids(env, enable_mtom, enable_swa,
                request_node, ht_attachments, AXIOM_MIME_TYPE_OCTET_STREAM TSRMLS_CC);
    }
    
    res_payload =  axis2_svc_client_send_receive(svc_client, env, request_node);

    response_envelope = axis2_svc_client_get_last_response_soap_envelope(svc_client, env);
    if (response_envelope) 
	{
        has_fault = AXIS2_TRUE;
        
		if (response_envelope)
            soap_body = axiom_soap_envelope_get_body (response_envelope, env);
        if (soap_body)
            soap_fault = axiom_soap_body_get_fault (soap_body, env);
        if (soap_fault) 
		{
            soap_version = axis2_options_get_soap_version(client_options, env);
            fault_node = axiom_soap_fault_get_base_node(soap_fault, env);
            if(fault_node)
			{
                res_text = axiom_node_to_string ( fault_node, env);
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI, "[wsf_wsdl]Fault payload is %s", res_text);
                MAKE_STD_ZVAL (rfault);
                INIT_PZVAL(rfault);

                object_init_ex (rfault, ws_fault_class_entry);
                
                add_property_stringl (rfault, WSF_MSG_PAYLOAD_STR, res_text, strlen (res_text), 1);
                
                wsf_util_handle_soap_fault(rfault, env, fault_node, soap_version TSRMLS_CC);
                zend_throw_exception_object(rfault TSRMLS_CC);
                return ;
            }
            
        }
        if (soap_body) 
		{
            body_base_node = axiom_soap_body_get_base_node(soap_body, env);
		}
        if (body_base_node && !soap_fault)
		{
            axis2_char_t *response_buffer = NULL;
            axis2_char_t *response_header_buffer = NULL;


            zval *cid2str = NULL;
            zval *cid2contentType = NULL;

            MAKE_STD_ZVAL (cid2str);
            MAKE_STD_ZVAL (cid2contentType);

            array_init (cid2str);
            array_init (cid2contentType);

            wsf_util_get_attachments_from_soap_envelope(env, response_envelope, cid2str, 
				cid2contentType TSRMLS_CC);

            soap_header = axiom_soap_envelope_get_header(response_envelope, env);
            if(soap_header) {
                header_base_node = axiom_soap_header_get_base_node(soap_header, env);

                if(axiom_node_get_first_child(header_base_node, env) != NULL) {
                    response_header_buffer = axiom_node_sub_tree_to_string(header_base_node, env);
                }
            }

            if(!response_header_buffer) {
                response_header_buffer = axutil_strdup(env, "");
            }

            res_params[0] = &res_param1;
            res_params[1] = &res_param2;
            res_params[2] = &res_param3;
            res_params[3] = &res_param4;
            res_params[4] = &res_param5;
          
            if(axiom_node_get_node_type(body_base_node, env) == AXIOM_ELEMENT ) {
                axiom_element_t *payload_element;
                axutil_qname_t *qname;
                payload_element = axiom_node_get_data_element(body_base_node, env);
                
                qname = axiom_element_get_qname(payload_element, env, body_base_node);
                if(axutil_strcmp(axutil_qname_get_localpart(qname, env), AXIOM_SOAP_BODY_LOCAL_NAME) != 0) {
                    res_payload = axiom_node_get_first_child(body_base_node, env);
                }
            }
            response_buffer = axiom_node_sub_tree_to_string(res_payload , env);

            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                             "[wsf_wsdl]Response buffer is %s", response_buffer);
            
            MAKE_STD_ZVAL(response_parameters);
            array_init(response_parameters);
            if(classmap)
                add_assoc_zval(response_parameters, WSF_WSDL_CLASSMAP, classmap);
            if(arguments)
                add_assoc_zval(response_parameters, WSF_WSDL_ARGS, arguments);

            add_assoc_zval (response_parameters, WSF_ATTACHMENTS , cid2str);
            add_assoc_zval (response_parameters, WSF_CID2CONTENT_TYPE , cid2contentType);

            ZVAL_STRING(&response_function, WSF_WSDL_RES_FUNCTION, 1);
            ZVAL_STRING(res_params[0], response_buffer, 1);
            INIT_PZVAL(res_params[0]);
            ZVAL_STRING(res_params[1], response_header_buffer, 1);
            INIT_PZVAL(res_params[1]);
            ZVAL_STRING(res_params[2], response_sig_model_string, 1);
            INIT_PZVAL(res_params[2]);
            ZVAL_ZVAL(res_params[3], response_parameters, NULL, NULL);
            INIT_PZVAL(res_params[3]);
            ZVAL_STRING(res_params[4], wsdl_dom_string, 1);
            INIT_PZVAL(res_params[4]);

            MAKE_STD_ZVAL(res_retval);
            INIT_PZVAL(res_retval);
            if (call_user_function (EG (function_table), (zval **) NULL,
                                    &response_function, res_retval, 5, 
                                    res_params TSRMLS_CC) == SUCCESS ){
                /* now the time to handle the res_retval */
                if(Z_TYPE_P(res_retval) == IS_ARRAY){
            
                    HashTable *ht_return = NULL;
                    zval **tmp = NULL;
                    
                    ht_return = Z_ARRVAL_P(res_retval);

                    if(zend_hash_find(ht_return, WSF_WSDL_RESPONSE_PAYLOAD_PARAM,
                                      sizeof(WSF_WSDL_RESPONSE_PAYLOAD_PARAM),
                                      (void **)&tmp) == SUCCESS) {
                        ZVAL_ZVAL(return_value, *tmp, 0, 0);
                    }
                    if(zend_hash_find(ht_return, WSF_WSDL_RESPONSE_HEADER_PARAM,
                                      sizeof(WSF_WSDL_RESPONSE_HEADER_PARAM),
                                      (void **)&tmp) == SUCCESS) {
                        if(arguments && Z_TYPE_P(arguments) == IS_ARRAY) {
                            /* keep iteration of proxy method argument */
                            HashPosition pos_arg;
                            zval **param_arg;

                            /* keep iteration of return value in processing response */
                            HashPosition pos_ret;
                            zval **param_ret;

                            zval *header_params = *tmp;

                            zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(header_params), &pos_ret);

                            /* iterate through all the arguments in the proxy method */
                            for (zend_hash_internal_pointer_reset_ex (Z_ARRVAL_P(arguments), &pos_arg);
                                    zend_hash_get_current_data_ex (Z_ARRVAL_P(arguments),
                                                            (void **) &param_arg, &pos_arg) == SUCCESS;
                                zend_hash_move_forward_ex (Z_ARRVAL_P(arguments), &pos_arg)) {
                                if((*param_arg)->is_ref) {
                                    /* we assign return header pointers to all the references
                                       called with the proxy method */
                                    if(zend_hash_get_current_data_ex (Z_ARRVAL_P(header_params),
                                                        (void **) &param_ret, &pos_ret) == SUCCESS) {
                                        ZVAL_ZVAL(*param_arg, *param_ret, 0, 0);
                                        zend_hash_move_forward_ex (Z_ARRVAL_P(header_params), &pos_ret);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, 
                         "[wsf_wsdl] Response envelope not found");
    }
    
}

axiom_soap_envelope_t* 
wsf_wsdl_create_request_envelope(char *payload,
                                 int soap_version,
                                 axutil_env_t *env TSRMLS_DC)
{
    axiom_soap_envelope_t* envelope = NULL;
	axiom_soap_header_t* header = NULL;
	axiom_soap_body_t* body = NULL;

	if ((soap_version == AXIOM_SOAP11) || (soap_version == AXIOM_SOAP12))
	{
		envelope = axiom_soap_envelope_create_with_soap_version_prefix(env, soap_version, AXIOM_SOAP_DEFAULT_NAMESPACE_PREFIX);
    } 
	else 
	{
        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, 
                         "[wsf_wsdl] Unknown SOAP version");
    }

    header = axiom_soap_header_create_with_parent(env, envelope);
    if (!header)
    {
        return NULL;
    }

    body = axiom_soap_body_create_with_parent(env, envelope);
    if (!body)
    {
        return NULL;
    }
    
	if (payload)
	{
		axiom_node_t* payload_axiom = NULL;

		payload_axiom = wsf_util_deserialize_buffer(env, payload);

		axiom_soap_body_add_child(body, env, payload_axiom);
	}
    return envelope;
}


axiom_soap_envelope_t *
wsf_wsdl_create_soap_envelope_from_buffer (
    axis2_char_t* buffer,
    const axutil_env_t * env,
    axis2_char_t * soap_version_uri)
{
    axiom_soap_envelope_t *soap_envelope = NULL;
    axiom_soap_builder_t *soap_builder = NULL;
    axiom_xml_reader_t *reader = NULL;
    axiom_stax_builder_t *builder = NULL;
    

    reader = axiom_xml_reader_create_for_memory (env, buffer, axutil_strlen(buffer), 
		AXIS2_UTF_8, AXIS2_XML_PARSER_TYPE_BUFFER);

    builder = axiom_stax_builder_create (env, reader);

    soap_builder = axiom_soap_builder_create (env, builder, soap_version_uri);

    soap_envelope = axiom_soap_builder_get_soap_envelope (soap_builder, env);

    return soap_envelope;
}


void wsf_wsdl_handle_client_security(
	 HashTable *client_ht,
     zval **policy_array,
     axutil_env_t *env,
     axis2_svc_client_t * svc_client TSRMLS_DC)
{
    zval *sec_token = NULL;
    zval *policy = NULL;
    zval **tmp = NULL;
    HashTable *policy_ht = NULL;
    
    axiom_node_t *binding_policy_node = NULL;
    axiom_node_t *input_policy_node = NULL;
    axiom_node_t *output_policy_node = NULL;

    zval **policy_options = NULL;
    rampart_context_t *rampart_ctx = NULL;

    neethi_policy_t *binding_neethi_policy = NULL;
    axiom_element_t *binding_policy_root_ele = NULL;
    neethi_policy_t *normalized_binding_neethi_policy = NULL;

    neethi_policy_t *input_neethi_policy = NULL;
    axiom_element_t *input_policy_root_ele = NULL;
    neethi_policy_t *normalized_input_neethi_policy = NULL;

    neethi_policy_t *merged_input_neethi_policy = NULL;
    
    axis2_svc_ctx_t *svc_ctx = NULL;
    axis2_conf_ctx_t *conf_ctx = NULL;
    axis2_conf_t *conf = NULL;
    axutil_param_t *security_param = NULL;
    axis2_desc_t *desc = NULL;
    axis2_policy_include_t *policy_include = NULL;
    axis2_svc_t *svc = NULL;

    char *policy_xml = NULL;

	/* If there are user defined policies it will be set instead of policies in WSDL */
	
    if (zend_hash_find (client_ht, WSF_POLICY_NAME, sizeof (WSF_POLICY_NAME),
                        (void **) &tmp) == SUCCESS) {
        policy = *tmp;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_client] policy object found ");
		
        if (Z_TYPE_P (policy) == IS_OBJECT) {
            policy_ht = Z_OBJPROP_P (policy);
            if (zend_hash_find (policy_ht, WSF_POLICY_XML, sizeof (WSF_POLICY_XML),
                	(void **) &tmp) == SUCCESS
            	&& (Z_TYPE_PP (tmp) == IS_STRING)) {
            	policy_xml = Z_STRVAL_PP (tmp);
              	input_policy_node = wsf_util_deserialize_buffer (env, policy_xml);
            }
			else{
			input_policy_node = wsf_do_create_policy (policy, AXIS2_FALSE, env TSRMLS_CC);
			}
        }
	
    }
    else{
    	HashTable *ht_policy = Z_ARRVAL_PP(policy_array);
        if(!ht_policy)
            return;
        
    	if(zend_hash_find(ht_policy, WSF_WSDL_OP_POLICY, sizeof(WSF_WSDL_OP_POLICY), 
                          (void **)&policy_options) == SUCCESS
           && Z_TYPE_PP(policy_options) == IS_STRING){
            binding_policy_node = 
            	wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                             "[wsf_wsdl]operation policy xml is  \n\t %s \n",
                             Z_STRVAL_PP(policy_options));
    	}
        
    	if(zend_hash_find(ht_policy, WSF_WSDL_IN_POLICY, sizeof(WSF_WSDL_IN_POLICY), 
            	          (void **)&policy_options) == SUCCESS
           && Z_TYPE_PP(policy_options) == IS_STRING){
            input_policy_node = 
            	wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                             "[wsf_wsdl]input policy xml is  \n\t %s \n",
                             Z_STRVAL_PP(policy_options));
    	}
        
    	if(zend_hash_find(ht_policy, WSF_WSDL_OUT_POLICY, sizeof(WSF_WSDL_OUT_POLICY),
                          (void **)&policy_options) == SUCCESS
           && Z_TYPE_PP(policy_options) == IS_STRING){
            output_policy_node = 
            	wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
            AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                             "[wsf_wsdl]output policy xml is  \n\t %s \n",
            	             Z_STRVAL_PP(policy_options));
    	}
    }
    
    if (zend_hash_find (client_ht, WSF_SECURITY_TOKEN,
                        sizeof (WSF_SECURITY_TOKEN), (void **) &tmp) == SUCCESS
        && Z_TYPE_PP (tmp) == IS_OBJECT) {
        sec_token = *tmp;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl] security token object found ");
    }
    
    if(!sec_token){
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]security object not found \n");
        return;
    }
	rampart_ctx = rampart_context_create (env);
    wsf_set_rampart_options (rampart_ctx, sec_token, NULL, env TSRMLS_CC);
    
    if (binding_policy_node) {
        if (axiom_node_get_node_type (binding_policy_node,
                                      env) == AXIOM_ELEMENT) {
            binding_policy_root_ele =
                (axiom_element_t *)
                axiom_node_get_data_element (binding_policy_node, env);
            
            if (binding_policy_root_ele) {
                binding_neethi_policy =
                    neethi_engine_get_policy (env, binding_policy_node,
                                              binding_policy_root_ele);
                normalized_binding_neethi_policy = 
                    neethi_engine_get_normalize(env, AXIS2_FALSE,binding_neethi_policy);
                neethi_policy_free(binding_neethi_policy, env);
                binding_neethi_policy = NULL;
            }
        }
    }
    
    if (input_policy_node) {
        if (axiom_node_get_node_type (input_policy_node,
                                      env) == AXIOM_ELEMENT) {
            input_policy_root_ele =
                (axiom_element_t *)
                axiom_node_get_data_element (input_policy_node, env);

            if (input_policy_root_ele) {
                input_neethi_policy =
                    neethi_engine_get_policy (env, input_policy_node,
                                              input_policy_root_ele);
                normalized_input_neethi_policy = 
                    neethi_engine_get_normalize(env, AXIS2_FALSE, 
                                                input_neethi_policy);
                neethi_policy_free(input_neethi_policy, env);
                input_neethi_policy = NULL;
            } 
        }
    }
    
    if(normalized_input_neethi_policy && normalized_binding_neethi_policy ){
        merged_input_neethi_policy = 
            neethi_engine_merge(env, normalized_input_neethi_policy, 
                                normalized_binding_neethi_policy);
        neethi_policy_free(normalized_binding_neethi_policy, env);
        neethi_policy_free(normalized_input_neethi_policy, env);
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]outgoing policies merged \n");
    }
    else if (!normalized_input_neethi_policy && normalized_binding_neethi_policy){
        merged_input_neethi_policy = normalized_binding_neethi_policy;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]binding policy is set \n");
    }
    
    else if (normalized_input_neethi_policy && !normalized_binding_neethi_policy){
        merged_input_neethi_policy = normalized_input_neethi_policy;
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]input policy is set \n");
    }
    
    else{
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]valid policies not found \n");
        return;
    }
    
    
    svc = axis2_svc_client_get_svc (svc_client, env);
    if (!svc) {
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]Service is NULL \n");
        return ;
    }
    desc = axis2_svc_get_base (svc, env);
    if (!desc) {
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]Description is NULL \n");
        return ;
    }
    policy_include = axis2_desc_get_policy_include (desc, env);
    if (!policy_include) {
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]Policy Include is NULL \n");
        return ;
    }
    axis2_policy_include_add_policy_element (policy_include, env,
                                             AXIS2_SERVICE_POLICY, 
                                             merged_input_neethi_policy);
    
    svc_ctx = axis2_svc_client_get_svc_ctx (svc_client, env);
    conf_ctx = axis2_svc_ctx_get_conf_ctx (svc_ctx, env);
    conf = axis2_conf_ctx_get_conf (conf_ctx, env);
    
    security_param = axutil_param_create (env, WSF_RAMPART_CONFIGURATION,
                             (void *) rampart_ctx);
    axis2_conf_add_param (conf, env, security_param);
    
    axis2_svc_client_engage_module (svc_client, env, WSF_MODULE_SECURITY);
    
    
}

void wsf_wsdl_process_service(axis2_char_t *wsdl_location, wsf_svc_info_t *svc_info, const axutil_env_t *env TSRMLS_DC)
{
    zval *params[2];
    zval retval, param1, param2;
    zval *param_array;
    zend_file_handle script;
    zval request_function;
    zval *operations;
    axutil_hash_index_t *hi = NULL;

    zval **policy_options = NULL;
    axis2_char_t *sig_model_string = NULL;
   
    FILE *new_fp;
    php_stream *stream;

    /* handles caching wsdl */
    int cache_wsdl;
    axis2_svc_ctx_t *svc_ctx = NULL;
    axis2_ctx_t *base_ctx = NULL;
 
    wsf_util_find_and_set_svc_ctx(env, svc_info, wsf_worker_get_conf_ctx(svc_info->php_worker, env));
   
    svc_ctx = svc_info->svc_ctx;
    cache_wsdl = svc_info->cache_wsdl;

    if(cache_wsdl && svc_ctx) {
        base_ctx = axis2_svc_ctx_get_base(svc_ctx, env);
        if(base_ctx) {
            sig_model_string = (axis2_char_t*)axis2_ctx_get_property(base_ctx, env, WSF_SVC_PROP_SIG_MODEL_STR);
            policy_options = (zval**)axis2_ctx_get_property(base_ctx, env, WSF_SVC_PROP_POLICY_ARRAY);
        }
    }

    /* if it is already exists in the service context just use them */
    if(sig_model_string && policy_options) {

        /* NOTE: if the cache exist, it pulled from the cache */
        svc_info->sig_model_string = sig_model_string;
        AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                        "[wsf_wsdl]using cached sig model string");
        wsf_wsdl_handle_server_security(svc_info, policy_options,
                                                        env TSRMLS_CC);
        AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                        "[wsf_wsdl]using cached policy_options");
    }
    else {
        
        /* NOTE: if not cached, it execute the default path */ 
        svc_info->sig_model_string = sig_model_string;
        params[0] = &param1;
        params[1] = &param2;
        
        MAKE_STD_ZVAL(param_array);
        array_init(param_array);
        add_assoc_string(param_array, WSF_WSDL, wsdl_location, 1);
        
		if(svc_info->auth_user && svc_info->auth_password && svc_info->auth_type)
		{
			add_assoc_string(param_array, WSF_HTTP_AUTH_PASSWORD, svc_info->auth_password, 1);
			add_assoc_string(param_array, WSF_HTTP_AUTH_USERNAME, svc_info->auth_user, 1);
			add_assoc_string(param_array, WSF_HTTP_AUTH_TYPE, svc_info->auth_type, 1);
		}
               
        MAKE_STD_ZVAL (operations);
        array_init (operations);
        if (svc_info->ops_to_functions) {
            for (hi =
                     axutil_hash_first (svc_info->ops_to_functions, env);
                 hi; hi = axutil_hash_next (env, hi)) {
                void *v = NULL;
                const void *k = NULL;
                axis2_char_t * f_key = NULL;
                axutil_hash_this (hi, &k, NULL, &v);
                f_key = (axis2_char_t *) k;
                add_next_index_string (operations, (char *)f_key , 1);
            } 
        }	
        
        
        ZVAL_STRING(&request_function, WSF_WSDL_SERVICE_REQ_FUNCTION, 0);
        
        if(!svc_info->generated_svc_name){
            add_assoc_string(param_array, WSF_SERVICE_NAME, svc_info->svc_name, 1);
        }
        if(svc_info->port_name){
            add_assoc_string(param_array, WSF_PORT_NAME, svc_info->port_name, 1);
        }
        
        ZVAL_ZVAL(params[0], param_array, NULL, NULL);
        INIT_PZVAL(params[0]);
        ZVAL_ZVAL(params[1], operations, NULL, NULL);
        INIT_PZVAL(params[1]);
        
        script.type = ZEND_HANDLE_FP;
        script.filename = WSF_WSDL_DYNAMIC_INVOC_SCRIPT;
        script.opened_path = NULL;
        script.free_filename = 0;


        stream  = php_stream_open_wrapper(WSF_WSDL_DYNAMIC_INVOC_SCRIPT, "rb", USE_PATH|REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);
        if(!stream)
            return;

        if (php_stream_cast(stream, PHP_STREAM_AS_STDIO|PHP_STREAM_CAST_RELEASE, (void*)&new_fp, REPORT_ERRORS) == FAILURE)    {
            AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, 
                             "[wsf_wsdl] Unable to open script file or file not found");
        }
        script.handle.fp =  new_fp;

        if(script.handle.fp){
            php_lint_script (&script TSRMLS_CC);
            if (call_user_function (EG (function_table), (zval **) NULL,
                                    &request_function, &retval, 2, 
                                    params TSRMLS_CC) == SUCCESS ){
                if (Z_TYPE_P(&retval) == IS_STRING){
                    svc_info->sig_model_string = Z_STRVAL_P(&retval);
                    AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                     "[wsf_wsdl]received data from scripts");
                }
                else if(Z_TYPE_P(&retval) == IS_ARRAY){
                    zval **tmp = NULL;
                    HashTable *ht_return = Z_ARRVAL_P(&retval);
                    
                    AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                    "[wsf_wsdl]received array from scripts");
                    if(zend_hash_find(ht_return, WSF_WSDL_SIG_MODEL, 
                                      sizeof(WSF_WSDL_SIG_MODEL), (void **)&tmp) == SUCCESS &&
                       Z_TYPE_PP(tmp) == IS_STRING){
                        svc_info->sig_model_string = Z_STRVAL_PP(tmp);
                        AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                        "[wsf_wsdl]received sig model string");
                    }
                    if(zend_hash_find(ht_return, WSF_WSDL_POLICIES, sizeof(WSF_WSDL_POLICIES), 
                                      (void **)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_ARRAY){
                        AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                        "[wsf_wsdl]policies found");
                        policy_options = tmp;
                        wsf_wsdl_handle_server_security(svc_info, policy_options,
                                                        env TSRMLS_CC);
                    }
                                    
                }
            }
        }

        if(cache_wsdl && svc_ctx) {
            base_ctx = axis2_svc_ctx_get_base(svc_ctx, env);
            if(base_ctx) {
                axutil_property_t *sig_model_prop = NULL;
                axutil_property_t *policy_options_prop = NULL;
                

                sig_model_prop = axutil_property_create(env);
                policy_options_prop = axutil_property_create(env);

                sig_model_prop = 
                  axutil_property_create_with_args(env, AXIS2_SCOPE_APPLICATION,
                                AXIS2_FALSE, 0, svc_info->sig_model_string);

                policy_options_prop = 
                  axutil_property_create_with_args(env, AXIS2_SCOPE_APPLICATION,
                                AXIS2_FALSE, 0, policy_options);

                axis2_ctx_set_property(base_ctx, env, 
                    WSF_SVC_PROP_SIG_MODEL_STR, sig_model_prop);
                AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                    "[wsf_wsdl]sig model string is cached");
                axis2_ctx_set_property(base_ctx, env, WSF_SVC_PROP_POLICY_ARRAY,
                                        policy_options_prop);
                AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                    "[wsf_wsdl]policy options array is cached");
            }
        }
    }
}

void wsf_wsdl_handle_server_security(wsf_svc_info_t *svc_info,
                                     zval **op_policies,
                                     const axutil_env_t *env TSRMLS_DC)
{
    HashTable *operations = NULL;
    HashTable *ht_policy = NULL;
    HashPosition pos;
    zval **tmp = NULL;
    zval **policy_options = NULL;
    
    
    axiom_node_t *binding_policy_node = NULL;
    axiom_node_t *input_policy_node = NULL;
    axiom_node_t *output_policy_node = NULL;
    
    rampart_context_t *rampart_ctx = NULL;
    
    neethi_policy_t *binding_neethi_policy = NULL;
    axiom_element_t *binding_policy_root_ele = NULL;
    neethi_policy_t *normalized_binding_neethi_policy = NULL;
    
    neethi_policy_t *input_neethi_policy = NULL;
    axiom_element_t *input_policy_root_ele = NULL;
    neethi_policy_t *normalized_input_neethi_policy = NULL;
    
    neethi_policy_t *merged_input_neethi_policy = NULL;
    int i = 0;
    axutil_param_t *security_param = NULL;
    axis2_conf_t *conf = NULL;
    wsf_worker_t *worker = NULL;

    if(!op_policies)
        return;
    operations = Z_ARRVAL_PP(op_policies);
    if(!operations)
        return;
    
  
    zend_hash_internal_pointer_reset_ex (operations, &pos);
    while (zend_hash_get_current_data_ex (operations,
                                          (void **) &tmp, &pos) != FAILURE) {
        char *op_name = NULL;
        unsigned int op_name_len = 0;
        unsigned long index = i;
        zend_hash_get_current_key_ex (operations, &op_name, &op_name_len, &index, 0, &pos);
        zend_hash_move_forward_ex (operations, &pos);
        i++;
        ht_policy = Z_ARRVAL_PP (tmp);
        if (op_name){

			axis2_op_t *op = NULL;
            axutil_qname_t *op_qname = NULL;
            axis2_desc_t *desc = NULL;
            
            axis2_policy_include_t *policy_include = NULL;
            AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI, "operation name is %s", op_name);
            /* begin_line */
            
            if(zend_hash_find(ht_policy, WSF_WSDL_OP_POLICY, sizeof(WSF_WSDL_OP_POLICY), 
                              (void **)&policy_options) == SUCCESS
				   && Z_TYPE_PP(policy_options) == IS_STRING){
                binding_policy_node = 
                    wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]operation policy xml is  \n\t %s \n",
                                 Z_STRVAL_PP(policy_options));
				   }
            
            if(zend_hash_find(ht_policy, WSF_WSDL_IN_POLICY, sizeof(WSF_WSDL_IN_POLICY), 
                              (void **)&policy_options) == SUCCESS
               && Z_TYPE_PP(policy_options) == IS_STRING){
                input_policy_node = 
                    wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]input policy xml is  \n\t %s \n",
                                 Z_STRVAL_PP(policy_options));
            }
            
            if(zend_hash_find(ht_policy, WSF_WSDL_OUT_POLICY, sizeof(WSF_WSDL_OUT_POLICY),
								  (void **)&policy_options) == SUCCESS
               && Z_TYPE_PP(policy_options) == IS_STRING){
                output_policy_node = 
                    wsf_util_deserialize_buffer (env, Z_STRVAL_PP(policy_options));
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]output policy xml is  \n\t %s \n",
										Z_STRVAL_PP(policy_options));
                
            }
            
            if (binding_policy_node) {
                if (axiom_node_get_node_type (binding_policy_node,
                                              env) == AXIOM_ELEMENT) {
                    binding_policy_root_ele =
                        (axiom_element_t *)
                        axiom_node_get_data_element (binding_policy_node, env);
                    
                    if (binding_policy_root_ele) {
                        binding_neethi_policy =
                            neethi_engine_get_policy (env, binding_policy_node,
                                                      binding_policy_root_ele);
                        normalized_binding_neethi_policy = 
                            neethi_engine_get_normalize(env, AXIS2_FALSE, 
                                                        binding_neethi_policy);
                        neethi_policy_free(binding_neethi_policy, env);
                        binding_neethi_policy = NULL;
                    }
                }
            }
            
            if (input_policy_node) {
                if (axiom_node_get_node_type (input_policy_node,
                                              env) == AXIOM_ELEMENT) {
                    input_policy_root_ele =
                        (axiom_element_t *)
                        axiom_node_get_data_element (input_policy_node, env);
                    
                    if (input_policy_root_ele) {
                        input_neethi_policy =
                            neethi_engine_get_policy (env, input_policy_node,
                                                      input_policy_root_ele);
                        normalized_input_neethi_policy = 
                            neethi_engine_get_normalize(env, AXIS2_FALSE, 
                                                        input_neethi_policy);
                        neethi_policy_free(input_neethi_policy, env);
                        input_neethi_policy = NULL;
                    } 
                }
            }
            
            if(normalized_input_neethi_policy && normalized_binding_neethi_policy ){
                merged_input_neethi_policy = 
                    neethi_engine_merge(env, normalized_input_neethi_policy, 
                                        normalized_binding_neethi_policy);
                neethi_policy_free(normalized_binding_neethi_policy, env);
					neethi_policy_free(normalized_input_neethi_policy, env);
					AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                                         "[wsf_wsdl]outgoing policies merged \n");
            }
            else if (!normalized_input_neethi_policy && normalized_binding_neethi_policy){
                merged_input_neethi_policy = normalized_binding_neethi_policy;
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]binding policy is set \n");
            }
				
            else if (normalized_input_neethi_policy && !normalized_binding_neethi_policy){
                merged_input_neethi_policy = normalized_input_neethi_policy;
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]input policy is set \n");
            }
            
            else{
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]valid policies not found \n");
                return;
            }
            /* end line */
            
            op_qname = axutil_qname_create (env, op_name, NULL, NULL);
            
            if (NULL != svc_info->svc && NULL != op_name) {
                op = axis2_svc_get_op_with_name (svc_info->svc, env, op_name);
                desc = axis2_op_get_base(op, env);
                policy_include = axis2_desc_get_policy_include(desc, env);
                axis2_policy_include_add_policy_element (policy_include, env,
                                                         AXIS2_SERVICE_POLICY, merged_input_neethi_policy);
                
            }
        }
        
    }	

    if(!svc_info->security_token){
        AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                         "[wsf_wsdl]security object not found \n");
        return;
    }
    rampart_ctx = rampart_context_create (env);
    
	wsf_set_rampart_options (rampart_ctx,  svc_info->security_token, NULL, env TSRMLS_CC);
    
	security_param = axutil_param_create (env, WSF_RAMPART_CONFIGURATION, (void *) rampart_ctx);
    axis2_svc_add_param (svc_info->svc, env, security_param);
    
    /** engage module rampart */

    worker = svc_info->php_worker;
				
    conf = axis2_conf_ctx_get_conf (wsf_worker_get_conf_ctx(worker, env), env);
    wsf_util_engage_module (conf, "rampart", env, svc_info->svc);
}



void wsf_wsdl_set_sig_model(char *wsdl_path, wsf_svc_info_t *svc_info, const axutil_env_t *env TSRMLS_DC)
{
    zval *params[2];
    zval retval, param1, param2;
    zval *param_array;
    zend_file_handle script;
    zval request_function;
    zval *operations;
    axutil_hash_index_t *hi = NULL;
   
    FILE *new_fp;
    php_stream *stream;
 
    params[0] = &param1;
    params[1] = &param2;
    
    MAKE_STD_ZVAL(param_array);
    array_init(param_array);
    add_assoc_string(param_array, WSF_WSDL, wsdl_path, 1);
    
    
    MAKE_STD_ZVAL (operations);
    array_init (operations);
    if (svc_info->ops_to_functions) {
        for (hi =
                 axutil_hash_first (svc_info->ops_to_functions, env);
             hi; hi = axutil_hash_next (env, hi)) {
            void *v = NULL;
            const void *k = NULL;
            axis2_char_t * f_key = NULL;
            axutil_hash_this (hi, &k, NULL, &v);
            f_key = (axis2_char_t *) k;
            add_next_index_string (operations, (char *)f_key , 1);
        } 
    }	
    
    
    ZVAL_STRING(&request_function, WSF_WSDL_SERVICE_REQ_FUNCTION, 0);
    
    if(!svc_info->generated_svc_name){
        add_assoc_string(param_array, WSF_SERVICE_NAME, svc_info->svc_name, 1);
    }
    if(svc_info->port_name){
        add_assoc_string(param_array, WSF_PORT_NAME, svc_info->port_name, 1);
    }
    
    ZVAL_ZVAL(params[0], param_array, NULL, NULL);
    INIT_PZVAL(params[0]);
    ZVAL_ZVAL(params[1], operations, NULL, NULL);
    INIT_PZVAL(params[1]);
    
    script.type = ZEND_HANDLE_FP;
    script.filename = WSF_WSDL_DYNAMIC_INVOC_SCRIPT;
    script.opened_path = NULL;
    script.free_filename = 0;

    stream  = php_stream_open_wrapper(WSF_WSDL_DYNAMIC_INVOC_SCRIPT, "rb", USE_PATH|REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);
    if(!stream)
        return;

    if (php_stream_cast(stream, PHP_STREAM_AS_STDIO|PHP_STREAM_CAST_RELEASE, (void*)&new_fp, REPORT_ERRORS) == FAILURE)    {

        AXIS2_LOG_ERROR(env->log, AXIS2_LOG_SI, 
                         "[wsf_wsdl] Unable to open script file or file not found");
    }
    script.handle.fp =  new_fp;

    if(script.handle.fp){
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]start linting");
        php_lint_script (&script TSRMLS_CC);
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]finished linting");
        if (call_user_function (EG (function_table), (zval **) NULL,
                                &request_function, &retval, 2, 
                                params TSRMLS_CC) == SUCCESS ){
            if (Z_TYPE_P(&retval) == IS_STRING){
                svc_info->sig_model_string = Z_STRVAL_P(&retval);
                AXIS2_LOG_DEBUG (env->log, AXIS2_LOG_SI,
                                 "[wsf_wsdl]received data from scripts");
            }
            else if(Z_TYPE_P(&retval) == IS_ARRAY){
				zval **tmp = NULL;
                HashTable *ht_return = Z_ARRVAL_P(&retval);
                
                AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                "[wsf_wsdl]received array from scripts");
                if(zend_hash_find(ht_return, WSF_WSDL_SIG_MODEL, 
                                  sizeof(WSF_WSDL_SIG_MODEL), (void **)&tmp) == SUCCESS &&
                   Z_TYPE_PP(tmp) == IS_STRING){
                    svc_info->sig_model_string = Z_STRVAL_PP(tmp);
                    AXIS2_LOG_DEBUG(env->log, AXIS2_LOG_SI,
                                    "[wsf_wsdl]received sig model string");
                }
            }
        }
    }
    
    
}
