#include "util_hudson.h"

// geth --rpc </dev/null >&1 &rpc":"2

// curl -H "Content-Type: application/json" -X POST --data '{"jsonrpc":"2.0","method":"web3_clientVersion","params":[],"id":67}' http://localhost:8545

long int first_block = 142;

long int latest_block = -1;

json_object* geth_call(string method, json_object* params){
	/* create json object for post */
    json_object * json = json_object_new_object();
    /* build post data */
    json_object_object_add(json, "jsonrpc", json_object_new_string("2.0"));
    json_object_object_add(json, "method", json_object_new_string(method));
    json_object_object_add(json, "params", params);
    json_object_object_add(json, "id", json_object_new_int64(17));
    json_object * output = get_json_from_post("http://localhost:8545", json);
	return find_something(output, "result");
}

long int get_latest_block(){
	char url[128];
	strcpy(url, "https://etherscan.io/blocks");
	string page = get_html_from_post(url, NULL)->payload;
	/* Compile a simple regular expression using character classes, meta-char and greedy + non-greedy quantifiers: */
	re_t pattern = re_compile("/block/");
	/* Check if the regex matches the text: */
	int starting_idx = re_matchp(pattern, page) + 7;
	int ending_idx = look_for_to_char(page, '\'', starting_idx);
	page[ending_idx] ='\0';
	return atoi(page + starting_idx);
}

long int get_time_of_block(long int block_num){
	json_object * parameters = json_object_new_array();
	char buff[32] = "";
    json_object_array_add(parameters, json_object_new_string(dec_to_hex(block_num, buff)));
    json_object_array_add(parameters, json_object_new_boolean(false));
	json_object* result = geth_call("eth_getBlockByNumber", parameters);
	if(!json_object_is_type(result, json_type_null)){
		return hex_to_int(json_object_get_string(find_something(result, "timestamp")));
	}
	char url[128];
	strcpy(url, "https://etherscan.io/block/");
	sprintf(url + strlen(url), "%ld", block_num);
	string page = get_html_from_post(url, NULL)->payload;
	/* Compile a simple regular expression using character classes, meta-char and greedy + non-greedy quantifiers: */
	re_t pattern = re_compile("UTC");
	/* Check if the regex matches the text: */
	int ending_idx = re_matchp(pattern, page);
	int starting_idx = ending_idx - 25;
	page[ending_idx + 3] ='\0';
	char date_string[64]; date_string[0] = '\0';
	strcat(date_string, page + starting_idx);
	url[0] = '\0';
	free(page);
	return string_to_epoch(date_string);
}

long int get_block_of_time(long int timestamp){
	if(latest_block == -1) latest_block = get_latest_block();
	long int first = first_block;
   	long int last = latest_block;
   	long int middle = (first+last)/2;
 
   	while (first <= last) {
   		long int value = get_time_of_block(middle);
      	if (value < timestamp)
        	first = middle + 1;    
      	else if (value == timestamp) {
        	return middle;
         	break;
      	}
      	else
        	last = middle - 1;
 
    	middle = (first + last)/2;
   	}
    return middle;
}

typedef struct transaction {
	char tx[128];
	char time[64];
	char from[64];
	char to[64];
	float amount;
	size_t pos;
} tx;
int heap_size = 10;

tx* new_tx(char* _tx, char* _time, char* _from, char* _to, float _amount){
	tx* t = (tx*) malloc(sizeof(tx));
	strcpy(t->tx, _tx);
	strcpy(t->time, _time);
	strcpy(t->from, _from);
	strcpy(t->to, _to);
	t->amount = _amount;
	return t;
}

pqueue_t *pq;


void extract_tx_from_page(string page){
	string holder = page;
	holder = strstr(holder, "<tr>") + 1;
	holder = strstr(holder, "<tr>");
	char tx[128];
	char time[64];
	char from[64];
	char to[64];
	char amount[64];
	//go through each row
	for(;holder != NULL; holder = strstr(holder, "<tr")){
		char* spot = strstr(holder, "href='/tx/");
		if(spot == NULL) break;
		char* tx_start = spot + 10;
		strncpy(tx, tx_start, 66);
		tx[66] = '\0';

		spot = strstr(holder, "bottom' title='");
		if(spot == NULL) break;
		char* date_start = spot + 15;
		strncpy(time, date_start, 23);
		time[60] = '\0';
		if(strstr(time, "Error") != NULL){ holder++; continue; }

		spot = strstr(holder, "href='/address/");
		if(spot == NULL) break;
		char* from_start = spot + 15;
		strncpy(from, from_start, 42);
		from[42] = '\0';

		spot++;

		spot = strstr(spot, "href='/address/");
		if(spot == NULL) break;
		char* to_start = spot + 15;
		strncpy(to, to_start, 42);
		to[42] = '\0';

		spot++;

		spot = strstr(spot, "<td>") + 4;
		char* spot2 = strstr(spot + 1, " Ether");
		if(spot == NULL) break;
		char* amount_start = spot;
		char amount_buff[64];
		strncpy(amount_buff, amount_start, (spot2 - spot));
		amount_buff[(spot2 - spot)] = '\0';
		char* x = amount_buff;
		int j = 0;
		int k = 0;
		for(; amount_buff[j] != '\0'; ++j)
	    {
	    	if(amount_buff[j] != '<' && amount_buff[j] != '>' && amount_buff[j] != '/' && amount_buff[j] != 'b'){
	        amount[k] = amount_buff[j];k++;}
	    }
	    amount[k] = '\0';
	    if(strcmp(amount, "0") == 0){
	    	holder++;
	    	continue;
	    }

	    float price = atof(amount);
	    //place_in_heap(tx, time, from, to, price);
		//printf("%s,%s,%s,%s,%s\n", tx, time, from, to, amount);
	    pqueue_insert(pq, new_tx(tx, time, from, to, price));
		holder++;
	}
}

void process_block_for_transactions(long int block_num){
	json_object * parameters = json_object_new_array();
	char buff[32] = "";
    json_object_array_add(parameters, json_object_new_string(dec_to_hex(block_num, buff)));
    json_object_array_add(parameters, json_object_new_boolean(true));
	json_object* result = geth_call("eth_getBlockByNumber", parameters);
	if(!json_object_is_type(result, json_type_null)){
		const char * list = json_object_get_string(find_something(result, "transactions"));
		printf("%ld %s\n", block_num, list);
	}
	char url[128];
	strcpy(url, "https://etherscan.io/txs?block=");
	sprintf(url + strlen(url), "%ld", block_num);
	string page = get_html_from_post(url, NULL)->payload;
	//get page count
	re_t pattern = re_compile(" of <b");
	int starting_idx = re_matchp(pattern, page) + 7;
	pattern = re_compile("</");
	int ending_idx = re_matchp(pattern, page + starting_idx) + starting_idx;
	char page_count_str[16];
	strncpy(page_count_str, page + starting_idx, ending_idx - starting_idx);
	page_count_str[ending_idx - starting_idx] = '\0';
	if(strstr(page, "There are no matching entries") != NULL){
		free(page);
		return;
	}
	//get first page
	extract_tx_from_page(page);
	free(page);
	int page_num = 2;
	for(; page_num <= atoi(page_count_str); page_num++){
		strcpy(url, "https://etherscan.io/txs?block=");
		sprintf(url + strlen(url), "%ld&p=%d", block_num, page_num);
		string page = get_html_from_post(url, NULL)->payload;
		extract_tx_from_page(page);
		free(page);
	}
}

static int
cmp_pri(pqueue_pri_t next, pqueue_pri_t curr)
{
	return (next < curr);
}


static pqueue_pri_t
get_pri(void *a)
{
	return ((tx *) a)->amount;
}


static void
set_pri(void *a, pqueue_pri_t pri)
{
	((tx *) a)->amount = pri;
}


static size_t
get_pos(void *a)
{
	return ((tx *) a)->pos;
}


static void
set_pos(void *a, size_t pos)
{
	((tx *) a)->pos = pos;
}

int main(int argc, string* arguments){

	
	tx   *ns;
	tx   *n;
	heap_size = atoi(arguments[3]);

	pq = pqueue_init(heap_size + 1, cmp_pri, get_pri, set_pri, get_pos, set_pos);

	
	process_block_for_transactions(5482750);
	process_block_for_transactions(5480500);
	process_block_for_transactions(5473001);
	process_block_for_transactions(5433005);
	
	
    int row = 0;
	while ((n = pqueue_pop(pq)) && pqueue_size(pq) > 0){
		printf("%f [%s %s]\n", n->amount, n->from, n->to);
		row++;
	}

	pqueue_free(pq);
}