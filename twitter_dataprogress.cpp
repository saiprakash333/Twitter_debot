#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sstream>
#include <json/json.h>
#include <json/forwards.h>
#include <ctime>
#include <cassert>
#include <vector>

extern "C" {
  #include <oauth.h>
}
using namespace std;

// ***** global ******

    #define DOWNLOAD_LIMIT 20000000

// structure for activity stored in memory

struct MemoryStruct {
    char *memory;
    size_t size;
};

// hash map for the time series activity stored

map<string, vector<long>> time_series;

// hash map for the buckets to store users in

map<int, vector<long>> hash_buckets;

// vector for valid users 

vector<long> valid_users;
vector<long> temp_users;

// ==== global variables and pointers used by the function

struct MemoryStruct chunk; 

// *************** functions **************


// call back function to store the json data to an array

size_t fncCallback(char* ptr, size_t size, size_t nmemb, void* stream) 
{
  
    size_t iRealSize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct* )stream;
    char* contents = (char* )realloc(mem->memory, mem->size + iRealSize + 1); // increase the size of the pointer
    
    if(contents == NULL) {
    printf("not enough memory (realloc returned NULL)\n"); /* out of memory! */ 
    return 0;
    }
     
    mem->memory = contents; // then put it back to mem MemoryStruct
    memcpy(&(mem->memory[mem->size]), ptr, iRealSize); // Copy data from ptr to our mem MemoryStruct starting from size
    mem->size += iRealSize; //Increment size for the next iteration
    mem->memory[mem->size] = 0; // keep the last bit as zero

    return iRealSize;
}


// function to check the download and stop the stream

static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{

    if(dlnow > DOWNLOAD_LIMIT)
     return 1;
    return 0;
}

// function to parse the json data and make the global user time array

void parse_jsonto_array()
{

    Json::Value root;
    Json::Reader reader;
    std::istringstream iss(chunk.memory);
    std::string line;
    string cur_line;
    bool success=1;
    int no_of_elements=0;
   
    while(success) 
    {
        getline(iss, cur_line, '\n');
        success = reader.parse(cur_line, root);
	string s = root["timestamp_ms"].asString();   // convert timestamp json value to string
	string ID = root["user"]["id"].asString();    // convert ID json value to string	
	
	if(!success)			// check for unsuccessful parsing
	break;

	if(s == "")			// check for in valid timestamp entries and continue
	continue;

	//getting rid of double quotes in the timestamp string and converting to long int 
	    s.erase( 0,0 ); 		// erase the first character
	    s.erase( s.size() ); 	// erase the last character
	
	long time = stol(s);

	time_series[ID].push_back(time);
    	
        no_of_elements++;
    }

    cout << "no_of_elements: " << no_of_elements << endl;
    cout << "done parsing" << endl;
    cout << "No of users found " << time_series.size() << endl;
}

    //cout << "Done" << endl;
    //cout << i->first << endl;
    //cout << time << endl;
    //cout << (i->second).size() << endl;	 	//debug step
    //cout << (i->second).size() << endl;	 	//debug step
    //cout << sizeof(i->second) << endl;
    //cout << *time_series.find(ID) << endl; 		//debug step
    //cout << &time_series[ID] << endl;	 		//debug step

	//map<string, vector<long>>::iterator k = time_series.find(ID);
	//cout << k->first << endl;
	//for (auto it = (k->second).begin(); it != (k->second).end(); it++)
	//{
	//	if (*it != time)
	//	{(time_series[ID]).push_back(time);
	//	no_of_elements++;}
    	//}
 
    
     //map<string, vector<long>>::iterator i;
     //vector<long> vec;
     //for (i = time_series.begin(); i != time_series.end(); i++)
     //{
     //    if ((i->second).size() > 1)
     //    {
     //	      cout << i->first << endl;
     //	      vec = i->second;
     //          for (long& stamps: vec) 
     //	      	cout << stamps << " ";
     //	      cout << endl;
     //    }
     //}


// function to remove the users who have less activity

void filter_unactive_users()
{
     map<string, vector<long>>::iterator it; 
     for (it = time_series.begin(); it != time_series.end(); it++)
	{
	if ((it->second).size() < 2)
	time_series.erase(it);
	}

     cout << "No of active users " << time_series.size() << endl;
}



// The function for sorting the users into hashbuckets using the random time series "r"

void hash_the_users(int no_bins, long allowable_lag)
{

    // picking a random time series for reference 
    
    map<string, vector<long>>::iterator it = time_series.begin();
    advance(it, rand() % (time_series.size()));
    string random_key = it->first;
    vector<long> ref_vec = it->second;

   // lag-sensitive hashing
  
    map<string, vector<long>>::iterator user;
  
    for (user = time_series.begin(); user != time_series.end(); user++)
    {
    	long lag_const = allowable_lag*1000;
	for (long lag = -lag_const; lag <= lag_const;)		
	{
		int correlation_count = 0;
    		for (long& ref_time: ref_vec)				//start with a single activity from the reference random series
		{
			vector<long> user_vec = user->second;
			for (long& user_time: user_vec)
			{
				if (abs(user_time-ref_time-lag) < 1000)     //if the reference is close to the user time then add the count to account for the match of that value
					correlation_count++;
				if ((user_time-ref_time-lag) > 12000)       //if the time is out of the scope of window then break the loop to avoid further calculations
					break;
			}
		}
	long user_long = stol(user->first);
	hash_buckets[correlation_count].push_back(user_long);           //add the user to the bucket if the correlation count matches
    	lag = lag + 1000;
	}
    }
    cout << "Number of hash buckets (or correlation values) created " << hash_buckets.size() << endl;
}
 
   
void valid_users_buckets(long allowable_lag)
{
    map<int, vector<long>>::iterator bucket;
    int threshold = allowable_lag/4;
    cout << "reached inside valid_users_buckets" << endl;
    for (bucket = hash_buckets.begin(); bucket != hash_buckets.end(); bucket++)
    {
	    int threshold_users = 0;
	    vector<long> users = bucket->second;
	    int bucket_size = bucket->second.size();
	    cout << "inside the hash bucket loop" << endl;
	    for (int i=0; i < bucket_size; i++)
	    {
		int count = 0;
		for (int j=0 ; j < bucket_size; j++)
		{	
			if (users[i] == users[j])
				count++;
	    	}
	    	if (count > threshold)
	    	{
		    threshold_users++;
	    	    temp_users.push_back(users[i]);
	    	}
    	    if (threshold_users < threshold)
    	    {   
	    	hash_buckets.erase(bucket);
    	    	temp_users.clear();
    	    }
    	    else
    	    {
    	    	valid_users.insert(valid_users.end(), temp_users.begin(), temp_users.end());
    	    }
	    }
    }
}
// ************* class and methods ***********

//proc defined 

class Proc
{
    const char* cUrl;
    const char* cConsKey;
    const char* cConsSec;
    const char* cAtokKey;
    const char* cAtokSec;
    CURL        *curl_handle;
    char*       cSignedUrl;
public:
    Proc(const char*, const char*, const char*, const char*, const char*);
    void execCurl(); //this method will copy the activity data to the memory struct
};

// Constructor for above
Proc::Proc(
    const char* cUrl,
    const char* cConsKey, const char* cConsSec,
    const char* cAtokKey, const char* cAtokSec)
{
    this->cUrl     = cUrl;
    this->cConsKey = cConsKey;
    this->cConsSec = cConsSec;
    this->cAtokKey = cAtokKey;
    this->cAtokSec = cAtokSec;
}


// ************* methods in classes defined ***********


void Proc::execCurl()
{

    chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */ 
    chunk.size = 0;    /* no data at this point */ 


    // ==== cURL Initialization
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    
    if (!curl_handle) {
        cout << "[ERROR] curl_easy_init" << endl;
        curl_global_cleanup();
        return;
    }

    // ==== cURL Setting
    // - URL, POST parameters, OAuth signing method, HTTP method, OAuth keys
    cSignedUrl = oauth_sign_url2(
        cUrl, NULL, OA_HMAC, "GET",
        cConsKey, cConsSec, cAtokKey, cAtokSec
    );
    // - URL
    curl_easy_setopt(curl_handle, CURLOPT_URL, cSignedUrl);
    // - User agent name /* some servers don't like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "mk-mode BOT");
    // - HTTP STATUS >=400 ---> ERROR
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
    // - Callback function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fncCallback);
    // - Write data
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    // - constant used to stop the stream
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0);
    // - function used to stop the stream
    curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, xferinfo);
    

    // ==== Execute
    int iStatus = curl_easy_perform(curl_handle);
    if (!iStatus)
        cout << "[ERROR] curl_easy_perform: STATUS=" << iStatus << endl;
    
    
    // ==== cURL Cleanup
    curl_easy_cleanup(curl_handle);
    // ==== cleanup
    curl_global_cleanup();
    
    return;
}

int main(int argc, const char *argv[])
{
      
    // ==== Constants - URL
    const char *URL = "https://stream.twitter.com/1.1/statuses/filter.json?track=youtube,instagram,facebook";
    // ==== Constants - Twitter kyes
    const char *CONS_KEY = "rYgVF9ovaS2G3dvolb42fOuFP";
    const char *CONS_SEC = "rk8Woyyezgu9NKCSGCebjoIF22lu3270b2V0y6BDWcvzetaYze";
    const char *ATOK_KEY = "136215989-zEv3d3WPFgHXMSqigAwXMWZ7mWzHmCHzb6bDD9GD";
    const char *ATOK_SEC = "05bH9wcco2vxTueMcAcji03schUTkqCYfNbYvH7SxKuq9";

    // ==== Instantiation
    Proc objProc(URL, CONS_KEY, CONS_SEC, ATOK_KEY, ATOK_SEC);
 
    // ==== step 1 of getting the timeline from twitter
    objProc.execCurl();
    printf("step 1 done %lu bytes retrieved\n", (unsigned long)chunk.size);
 
    // ==== step 2 appeding the data to array
    parse_jsonto_array();
    printf("step 2 parsing the data done\n");

    // ==== step 3 remove the users who have less activity 
    filter_unactive_users();
    printf("step 3 done filtering unactive users\n");

    // ==== step 4 hashing the users into buckets
    int no_bins = 5000;
    long allowable_lag = 20; // in seconds
    hash_the_users(no_bins, allowable_lag);
    printf("step 4 done hashing the the users\n");

    // ==== step 5 find the vaild users and the valid buckets
    valid_users_buckets(allowable_lag);
    printf("step 5 done finding the valid suspicious users\n");
    
    // ==== step 6 listen to the suspicious users

    return 0;

}
