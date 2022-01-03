/* Name: mymalloc.c -- Lab6 COSC 360
 * Author: Thomas Neuefeind
 * Date:11/2/2021
 * Description: This program emulates cstdlib's malloc by using sbrk() to allocate memory
 */

#include "mymalloc.h"

// C
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// total of 16 bytes
struct block {
	unsigned int size;    //4 bytes
						  //4 NULL bytes
	struct block * flink; //8 bytes
};

struct block * head = NULL; // the one global variable -- a pointer to the head of the linked list of blocks (i.e. free_list)

/**
 * @name my_malloc.
 * @brief Allocates memory using sbrk to emulate cstdlib's malloc function.
 * @param[in] malloc_size The chunk of memory to be allocated to the user.
 * @param[out] pointer to the chunk of memory that was allocated to the user.
 * @return pointer to the chunk of memory that was allocated to the user.
 */
void* my_malloc(size_t malloc_size) {

	//check for invalid malloc size
	if(malloc_size <= 0){
		fprintf(stdout, "invalid malloc size");
		exit(1);
	}

	malloc_size = (malloc_size + 7 + 8) & -8; // taken from Dr. Marz's lecture notes... Ensures that size is at least 16 and a multiple of 8

	//variables to keep track of the free list:
	//free_ptr keeps track of the chunk to be allocated from the free list
	struct block * free_ptr;
	struct block * curr_node;
	void * find_ptr;
	struct block * previous_node = NULL; //second to last node on free list...
	
	//look for a chunk of sufficient size on the free list. If there is not a free_list yet then free_list_begin() returns NULL and the loop will not execute
	for (find_ptr = free_list_begin(); find_ptr != NULL; find_ptr = free_list_next(find_ptr)) {
		curr_node = (struct block *) find_ptr;
		if(curr_node->size >= malloc_size){
			if (curr_node->size - malloc_size == 8) malloc_size += 8;
			break; //break if the current node has enough space
		}
		previous_node = curr_node; //update the previous with the current node, this way previous_node point to last allocated chunk in the free_list
	}

	//sbrk_ptr gets the void return value of sbrk. Later cast to a block *
	void * sbrk_ptr;


	//not a big enough chunk on free list, free_list may also be empty
	if (find_ptr == NULL){

		//get the correct size for the sbrk call
		int total_size;
		if(malloc_size > 8192){
			total_size = malloc_size;
		}
		else { //malloc_size <= 8192
			total_size = 8192;
		}

		sbrk_ptr = (void *) sbrk(total_size);
		free_ptr = (struct block *) sbrk_ptr; //returns the address of the next chunk to be allocated
		free_ptr->size = total_size;
		free_ptr->flink = NULL;

		if(head == NULL) { //add initial memory to free_list 
			head = free_ptr; 
		}
		else { //free list exists, but there isn't a chunk big enough left... so set the previous nodes's flink to the new chunk of memory
			previous_node->flink = free_ptr;
		}
	}

	//alloc_ptr is used to calculate the chunk to be allocated from the free list
	struct block * alloc_ptr;
	unsigned temp_size;


	if(find_ptr != NULL){ //found big enough chunk, so assign alloc_ptr with the address of the big enough chunk of free list
		alloc_ptr = curr_node;
		temp_size = curr_node->size;
	}
	else{ //not a chunk big enough, so had to call sbrk, giving alloc_ptr the address of that new chunk
		alloc_ptr = free_ptr;
		temp_size = free_ptr->size;
	}
	
	// ***alloc_ptr now stores the location of the chunk to be allocated***
	

	//  next, calculate offset
	void * alloc_offset = alloc_ptr;
	
	//if the free_list size and malloc_size are the same...
	if(alloc_ptr->size == malloc_size){		
		if(previous_node == NULL){
			head = alloc_ptr->flink;
		}
		else {
			previous_node->flink = alloc_ptr->flink;
		}
	}
	//free_list size and malloc are NOT the same
	else {
		free_ptr = (struct block *) (alloc_offset + malloc_size); //now points to the end of the chunk to be carved off
		free_ptr->size = temp_size - malloc_size;
		alloc_ptr->size = malloc_size;
		if(previous_node == NULL){ //first free_list node is big enough
			free_ptr->flink = alloc_ptr->flink;
			head = free_ptr;
		}
		else { //somewhere in the middle of the list	
			free_ptr->flink = alloc_ptr->flink;
			previous_node->flink = free_ptr;
		}
	}

	return alloc_offset + 8;
}

/**
 * @name my_free.
 * @brief Frees memory allocated by my_malloc.
 * @param[in] ptr The address of the memory chunk to be freed.
 */
void my_free(void* ptr) {
	
	ptr -= 8;
	
	struct block * to_free = (struct block *) ptr;
	struct block * find_ptr;
	struct block * previous_ptr = NULL;

	//search through the free list, keeping track of the previous node.
	for (find_ptr = free_list_begin(); find_ptr != NULL; find_ptr = free_list_next(find_ptr)) {
		if (find_ptr > to_free) break;
		previous_ptr = find_ptr;
	}

	if(head == NULL){
		head = to_free;
	}
	//if the previous_ptr is null, then the current node goes at the beginning of the free list
	else if(previous_ptr == NULL){
		struct block * curr_free_head = head;
		head = to_free;
		head->flink = curr_free_head;
	}
	//otherwise, the current node is nestled between the previous and next nodes
	else{
		to_free->flink = previous_ptr->flink;	
		previous_ptr->flink = to_free;
	}
}

//return the head of the free list
void* free_list_begin() {
	return head;
}

//return the current node's flink, or NULL if flink is NULL
void* free_list_next(void* node) {
	struct block * temp_node = (struct block *) node;
	return temp_node->flink;
}

/**
 * @name coalesce_free_list.
 * @brief checks for contiguous chunks on the free list and combines them
 * @param[in] none
 * @param[out] none
 * @return void function... but upon returning all contiguous chunks are combined
 */
void coalesce_free_list() {

	struct block * find_ptr;
	void * void_ptr;
	for (find_ptr = free_list_begin(); find_ptr != NULL;) {
		void_ptr = (void*) find_ptr;
		void_ptr += find_ptr->size;
		if (find_ptr->flink == void_ptr){
			find_ptr->size += ((struct block *) void_ptr)->size;
			find_ptr->flink = ((struct block *) void_ptr)->flink;
			continue;
		}
		find_ptr = free_list_next(find_ptr);
	}	
}
