/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_slowLog.h"

/* If you declare any globals in php_slowLog.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(slowLog)
*/

/* True global resources - no need for thread safety here */
static int le_slowLog;
static void (*backup_zend_execute_ex) (zend_execute_data *execute_data);

static time_t get_time_diff_ms(struct TIMEB *time_start,struct TIMEB *time_end){
	time_t t_sec,t_ms;
	t_sec=time_end->time-time_start->time;//计算秒间隔
	t_ms=time_end->millitm-time_start->millitm;//计算毫秒间隔
	return t_sec*1000+t_ms;
}


static char * get_file_class_function_lineno(zend_execute_data* data){
	char *file     = NULL;
	char *class    = NULL;
	char *function = NULL;
	char *ret      = NULL;
	int len;

	if (NULL==data||NULL==data->func){
		return ret;
	}

	//get class name
	if (data->This.value.obj) {
		if (data->func->common.scope) {
			if (strcmp(ZSTR_VAL(data->func->common.scope->name), "class@anonymous") == 0) {
				file = ZSTR_VAL(data->func->common.scope->info.user.filename);
				class = "anonymous-class";
			} else {
				class = ZSTR_VAL(data->func->common.scope->name);
			}
		} else {
			class = ZSTR_VAL(data->This.value.obj->ce->name);
		}
	} else {
		if (data->func->common.scope) {
			class = ZSTR_VAL(data->func->common.scope->name);
		}
	}

	//get file name function name
	if (data->func->common.function_name) {
		file = ZSTR_VAL(data->func->op_array.filename);
		if (strcmp(ZSTR_VAL(data->func->common.function_name), "{closure}") == 0) {
			function = "closure";
		} else {
			function = ZSTR_VAL(data->func->common.function_name);
		}
	} else if ( data->func->type == ZEND_EVAL_CODE &&
			data->prev_execute_data &&
			data->prev_execute_data->func &&
			data->prev_execute_data->func->common.function_name &&
			(
					(strncmp(ZSTR_VAL(data->prev_execute_data->func->common.function_name), "assert", 6) == 0) ||
					(strncmp(ZSTR_VAL(data->prev_execute_data->func->common.function_name), "create_function", 15) == 0)
			)
			) {
		file = ZSTR_VAL(data->func->op_array.filename);
		function = "internal-eval";
	} else if (
			data->prev_execute_data &&
			data->prev_execute_data->func->type == ZEND_USER_FUNCTION &&
			data->prev_execute_data->opline &&
			data->prev_execute_data->opline->opcode == ZEND_INCLUDE_OR_EVAL
			) {
		switch (data->prev_execute_data->opline->extended_value) {
			case ZEND_EVAL:
			case ZEND_INCLUDE:
			case ZEND_REQUIRE:
			case ZEND_INCLUDE_ONCE:
			case ZEND_REQUIRE_ONCE:
				// do nothing
				return NULL;
			default:
				// record
				file = ZSTR_VAL(data->func->op_array.filename);
				function = "???_op";
				break;
		}
	} else if (data->prev_execute_data) {
		return  get_file_class_function_lineno(data->prev_execute_data);
	} else {
		// record
		file = ZSTR_VAL(data->func->op_array.filename);
		function = "???_op";
	}

	if (function == NULL || strlen(function) <= 0) {
		return NULL;
	}

	if (file == NULL || strlen(file)<=0) {
		file = "-";
	}

	if (class == NULL || strlen(class) <= 0) {
		class = "-";
	}

	len = strlen(file) + strlen(class) + strlen(function)+24;
	ret = (char*)emalloc(len);
	snprintf(ret, len, "[file]%s{class}%s(function)%s", file, class, function);
	ret[len-1] = '\0';
	return  ret;
}

static long get_stack_depth(HashTable *htd,char *caller_name)
{
	zval* last_val=NULL;
	zend_string *key=NULL;

	if (NULL==htd || NULL=caller_name){
		return 0;
	}

	if(!zend_hash_str_exists(htd,caller_name,strlen(caller_name))){
		return 0;
	}

	key=zend_string_init(caller_name,strlen(caller_name),0);
	last_val= zend_hash_find(Z_ARRVAL_P(MYFILE_G(function_stack_map)), key);
	zend_string_release(key);

	if(last_val!=NULL) {
		return  Z_LVAL(*last_val);
	}

	return 0;
}

static void record_function_runtime_info(char* func_info, char*caller_info,time_t time_elapsed){
	HashTable *htd=HASH_OF(MYFILE_G(function_stack_map));
	HashTable *ht = HASH_OF(MYFILE_G(function_time_out_map));
	if(NULL==ht||NULL==htd||NULL==func_info){
		return;
	}


	if (!zend_hash_str_exists(ht, func_info, strlen(func_info))) {
			add_assoc_long(MYFILE_G(function_time_out_map), function_time_out_map, time_elapsed);
		}else{
		zend_string *key=zend_string_init(func_info,strlen(func_info),0);
		zval* last_val = zend_hash_find(Z_ARRVAL_P(MYFILE_G(function_time_out_map)),key);
		zend_string_release(key);
		key=NULL;
		if(last_val!=NULL && time_elapsed>Z_LVAL(*last_val)){
			add_assoc_long(MYFILE_G(function_time_out_map), func_info, time_elapsed);
		}
	}


	long depth=get_stack_depth(htd,func_info);//callee 最先执行

	long caller_depth=get_stack_depth(htd,caller_info);
	if (caller_depth<depth+1){
		caller_depth=depth+1;
		if (caller_info!=NULL){
			add_assoc_long(MYFILE_G(function_stack_map),caller_info,caller_depth);
		}
	}
	//add_assoc_long(MYFILE_G(function_stack_map),func_info,depth);

	return;
}
ZEND_DLEXPORT void slow_log_zend_execute_hook(zend_execute_data *execute_data){
	if (MYFILE_G(enable_slow_log)!=1) {
		backup_zend_execute_ex(execute_data);
		return;
	}

	if (NULL==execute_data){
		return;
	}

	struct TIMEB time_start,time_end;
	long len;
	time_t time_elapsed=0;
	char *func_info=NULL,*caller_info=NULL;
	zend_op_array *op_array=NULL;
	zend_execute_data *current_data=NULL, *prev_data=NULL;

	ftime(&time_start);//开始计时

	if (NULL!=execute_data->func){
		op_array = &(execute_data->func->op_array);
	}

	if (!(EX(func)->op_array.fn_flags & ZEND_ACC_GENERATOR)) {
		EX(opline) = EX(func)->op_array.opcodes;
	}

	prev_data= execute_data->prev_execute_data;//diff current_data->prev_execute_data;

	/* If we're in a ZEND_EXT_STMT, we ignore this function call as it's likely
       that it's just being called to check for breakpoints with conditions */
	if (prev_data && prev_data->func && ZEND_USER_CODE(prev_data->func->type) && prev_data->opline && prev_data->opline->opcode == ZEND_EXT_STMT) {
		my_zend_execute_ex(execute_data);
		ftime(&time_end);//停止计时
		return;
	}

	current_data = EG(current_execute_data);
	func_info = get_file_class_function_lineno(current_data);
	if (NULL==func_info) {
		backup_zend_execute_ex(execute_data);
		ftime(&time_end);//停止计时
		return;
	}

	slow_log_zend_execute_hook(execute_data);
	ftime(&time_end);//停止计时
	time_elapsed=get_time_diff_ms(&time_start,&time_end);

	if (current_data && current_data->prev_execute_data) {
		caller_info = get_file_class_function_lineno(current_data->prev_execute_data);
	}

	record_function_runtime_info(func_info,caller_info,time_elapsed);
	if(func_info!=NULL){
		efree(func_info);
	}

	if (caller_info!=NULL){
		efree(caller_info);
	}
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("slowLog.enable_slow_log","0", PHP_INI_ALL, OnUpdateLong, enable_slow_log, zend_slowLog_globals, slowLog_globals)
    STD_PHP_INI_ENTRY("slowLog.min_time_out_ms","80", PHP_INI_ALL, OnUpdateLong, min_time_out_ms, zend_slowLog_globals, slowLog_globals)
    STD_PHP_INI_ENTRY("slowLog.slow_log_dir", "/usr/local/var/log/slow_php_function.log", PHP_INI_ALL, OnUpdateString, slow_log_dir, zend_slowLog_globals, slowLog_globals)
PHP_INI_END()
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_slowLog_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_slowLog_compiled)
{
	char *arg = NULL;
	size_t arg_len, len;
	zend_string *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "slowLog", arg);

	RETURN_STR(strg);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_slowLog_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_slowLog_init_globals(zend_slowLog_globals *slowLog_globals)
{
	slowLog_globals->global_value = 0;
	slowLog_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(slowLog)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(slowLog)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(slowLog)
{
#if defined(COMPILE_DL_SLOWLOG) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	backup_zend_execute_ex=zend_execute_ex;
	zend_execute_ex=slow_log_zend_execute_hook;

	if (MYFILE_G(function_time_out_map)) {
		zval_ptr_dtor(MYFILE_G(function_time_out_map));
		efree(MYFILE_G(function_time_out_map));
	}
	MYFILE_G(function_time_out_map) = (zval *)emalloc(sizeof(zval));
	array_init(MYFILE_G(function_time_out_map));

	if (MYFILE_G(function_stack_map)) {
		zval_ptr_dtor(MYFILE_G(function_stack_map));
		efree(MYFILE_G(function_stack_map));
	}
	MYFILE_G(function_stack_map) = (zval *)emalloc(sizeof(zval));
	array_init(MYFILE_G(function_stack_map));
	return SUCCESS;
}
/* }}} */

static void save_slow_func_log(){
	FILE *fp=NULL;
	char *mode="a+";
	zend_ulong num_key=0;
	zend_string* string_key=NULL,*result=NUL;
	zval *entry=NULL,*lastVal=NULL;
	if(NULL==MYFILE_G(slow_log_dir)||""==MYFILE_G(slow_log_dir)){
		return;
	}
	fp = VCWD_FOPEN(MYFILE_G(slow_log_dir), mode;
	if(NULL==fp){
		return;
	}


	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(MYFILE_G(function_time_out_map)), num_key, string_key, entry) {
		if(MYFILE_G(mim_time_out_ms)>Z_LVAL(*entry)) {
			continue;
		}

		lastVal = zend_hash_find(Z_ARRVAL_P(MYFILE_G(function_stack_map)),string_key);
		long depth=0;
		if (lastVal!=NULL){
			depth=Z_LVAL(*lastVal);
		}

		char * tabs=emalloc(depth+1);
		if (tabs!=NULL){
			int i=0;
			for (i=0;i<depth;i++){
				tabs[i]=' ';
			}
			tabs[depth]='\0';
			result = strpprintf(0, "%s%s:%dms\n",tabs,ZSTR_VAL(string_key), Z_LVAL(*entry));
			efree(tabs);
			tabs=NULL;
		}else{
			result = strpprintf(0, "%s:%dms\n",ZSTR_VAL(string_key), Z_LVAL(*entry));
		}

		if (NULL!=result) {
			fwrite(result, strlen(result) * sizeof(char), 1, fp);
			efree(result);
			result=NULL;
		}

	} ZEND_HASH_FOREACH_END();

	close(fp);
	fp=NULL;
	return;
}

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(slowLog)
{

	zend_execute_ex=backup_zend_execute_ex;

	save_slow_func_log();

	if (MYFILE_G(function_time_out_map)) {
		zval_ptr_dtor(MYFILE_G(function_time_out_map));
		efree(MYFILE_G(function_time_out_map));
		MYFILE_G(function_time_out_map) = NULL;
	}

	if (MYFILE_G(function_stack_map)) {
		zval_ptr_dtor(MYFILE_G(function_stack_map));
		efree(MYFILE_G(function_stack_map));
		MYFILE_G(function_stack_map) = NULL;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(slowLog)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "slowLog support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ slowLog_functions[]
 *
 * Every user visible function must have an entry in slowLog_functions[].
 */
const zend_function_entry slowLog_functions[] = {
	PHP_FE(confirm_slowLog_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in slowLog_functions[] */
};
/* }}} */

/* {{{ slowLog_module_entry
 */
zend_module_entry slowLog_module_entry = {
	STANDARD_MODULE_HEADER,
	"slowLog",
	slowLog_functions,
	PHP_MINIT(slowLog),
	PHP_MSHUTDOWN(slowLog),
	PHP_RINIT(slowLog),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(slowLog),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(slowLog),
	PHP_SLOWLOG_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SLOWLOG
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(slowLog)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
