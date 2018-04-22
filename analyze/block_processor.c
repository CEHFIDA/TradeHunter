#include "pipe/pipe.h"
#include "pipe/pipe_util.h"

#include "util_hudson.h"

// geth --rpc </dev/null >&1 &rpc":"2

// curl -X POST --data '{"jsonrpc":"2.0","method":"web3_clientVersion","params":[],"id":67}' http://localhost:8545

string geth_call(string method, int num_args, ...){
	int *arg_count;
	va_list ap;
	va_start(ap, num_args);
	/* create json object for post */
    json_object * json = json_object_new_object();
	int i = 0; string argument;
	for(; i < num_args; i++){
		argument = va_arg(argument, string);
		printf("%s", argument);
	}
    /* build post data */
    json_object_object_add(json, "jsonrpc", json_object_new_string("2.0"));

    json_object * json_array = json_object_new_array();

    json_object_object_add(json, "method", json_object_new_string(method));
    json_object_object_add(json, "params", json_object_new_int(67));

	return NULL;
}

long int get_time_of_block(long int block_num){
	return -1;
}

long int get_block_of_time(long int timestamp){
	return -1;
}

int main(int argc, string* arguments){
	if(argc < 3){
		printf("You need to assign two times.\n");
		exit(1);
	}
	long int start_stamp = string_to_epoch(arguments[1]);
	long int end_stamp = string_to_epoch(arguments[2]);
	if(end_stamp < start_stamp){
		swap(&end_stamp, &start_stamp);
	}
	printf("Your timestamps are %ld and %ld\n", start_stamp, end_stamp);

	geth_call("aa", 4, "bb", "cc", "dd", "ee");
}