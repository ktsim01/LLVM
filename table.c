#include "table.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

table_t *create_table(table_t *prev){
    table_t *current = malloc(sizeof(table_t));
    current->nexttable = prev;
    current->entrylist = NULL;
    return current;
}

void destroy_table(table_t *current){
    while(current->entrylist){
        entry_t* temp = current->entrylist;
        current->entrylist = current->entrylist->nextentry;
        free(temp->name);
        free(temp);
    }
    free(current);
}
void insert_value(table_t* table, char* name, value_t value)
{
    entry_t* entry = table->entrylist;
    //loop through table's entrylist, replace value if name already exists
    while(entry)
    {
        if(strcmp(entry->name,name)==0)
        {
            printf("identifier already defined!\n");
            exit(0);
        }
        entry = entry->nextentry;
    }

    //if name does not exist, make new entry with proper name/value and add to end of list
    entry_t* insertion;
    insertion = (entry_t*)malloc(sizeof(entry_t));
    insertion->nextentry = table->entrylist;
    insertion->name=name;
    insertion->val=value;

    table->entrylist=insertion;
}

bool contains_name(table_t* table, char* name){
    entry_t* entry = table->entrylist;
    //loop through the table's entrylist, look for name
    while(entry)
    {
        if(strcmp(entry->name,name)==0)
        {
            return true;
        }
        entry = entry->nextentry;
    }

    //at this point, entire entrylist has been searched
    return false;
}

value_t get_value(table_t* table, char* name)
{
    entry_t* entry = table->entrylist;

    //loop through entrylist, look for name, and return corresponding value
    while(entry)
    {
        if(strcmp(entry->name,name)==0)
        {
            return entry->val;
        }
        entry = entry->nextentry;
    }

    //recursively search all tables
    if(table->nexttable != NULL)
    {
        return get_value(table->nexttable, name);
    }

    //at this point, all tables and their entries have been searched, return default value
    value_t def;
    def.address=NULL;
    def.value=NULL;
    return def;
}

//make function that takes a char* and length, and replaces \n with a newline, \t with a tab, \\ with \, \' with ', and \" with "
//all followups to \ that arent one of those 5 is invalid, so throw error
char* translate_special_chars(char* str, int length)
{
    //create new string that will store the fixed version
    char* newstr = (char*)malloc(length);

    //to offset misalignment between newstr and str
    int counter=0;

    //run through all of str
    for(int i =0; i<length; i++)
    {
        //DEBUG: printf("i: %d, char: %c\n", i, str[i]);

        //see a /, check following char for special characters
        if(str[i]=='\\')
        {
            //DEBUG: printf("inside backslash\n");
            
            //edge case: \ found as last character; is invalid, throw error
            if(i==length-1){
                //error
                printf("last character error\n");
            } 

            //check following character
            i+=1;
            //newline
            if(str[i]=='n')
            {
                counter++;
                newstr[i-counter] = '\n';
            }
            //tab
            else if(str[i]=='t')
            {
                counter++;
                newstr[i-counter] = '\t';
            }
            //backslash
            else if(str[i]=='\\')
            {
                counter++;
                newstr[i-counter] = '\\';
            }
            //single quote
            else if(str[i]=='\'')
            {
                counter++;
                newstr[i-counter] = '\'';
            }
            //double quote
            else if(str[i]=='\"')
            {
                counter++;
                newstr[i-counter] = '\"';
            }
            //invalid entry
            else
            {
                //error, terminate(?)
                printf("invalid entry after backslash\n");
            }
        }
        //if not special char, just copy
        else
        {
            //DEBUG: printf("regular character\n");

            newstr[i-counter] = str[i];
        }
    }
    //resize the string
    char* retstr =  realloc(newstr, length-counter);

    //DEBUG: printf("%s", retstr);

    //return
    return retstr;
}
