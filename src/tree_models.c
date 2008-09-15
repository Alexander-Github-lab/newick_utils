/* functions for tree models */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdio.h>

#include "list.h"
#include "rnode.h"
#include "common.h"
#include "link.h"
#include "to_newick.h"
#include "tree_models.h"
#include "redge.h"

#define UNUSED -1

static int running_number()
{
	static int n = 0;
	return n++;
}

/******************************************************************/
/* Geometric model */


/* Determines if a node is to have children, based on a pseudorandom number */

static int geo_has_children(double prob_node_has_children)
{
	double rn = (double) rand() / RAND_MAX;

	if (rn <= prob_node_has_children)
		return TRUE;
	else
		return FALSE;
}

/* Visits a leaf: probabilistically adds children to the leaf, and adds those
 * children to the leaves queue (since they are new leaves) */

static void geo_visit_leaf(struct rnode *leaf, double prob_node_has_children,
		struct llist *leaves_queue)
{
	// printf ("visiting leaf %p (%s)\n", leaf, leaf->label);
	if (geo_has_children(prob_node_has_children)) {
		// printf (" gets children\n");
		struct rnode *kid1 = create_rnode("kid1");	
		struct rnode *kid2 = create_rnode("kid2");	
		link_p2c(leaf, kid1, "");
		link_p2c(leaf, kid2, "");
		append_element(leaves_queue, kid1);
		append_element(leaves_queue, kid2);
	} else {
		// printf (" gets no children\n");
	}
}

/* Generate a tree using the geometric model */

void geometric_tree(double prob_node_has_children)
{
	struct llist *leaves_queue = create_llist();
	struct rnode *root = create_rnode("root");

	append_element(leaves_queue, root);

	/* The queue contains any newly added leaves. We visit them in turn,
	 * possibly adding new leaves to th equeue. The process stops when no
	 * new leaves have been added. */

	while (leaves_queue->count > 0) {
		int nb_leaves_to_visit = leaves_queue->count;
		/* Iterate over leaves. Note that new leaves can be added at
		 * the end of the queue */
		for (; nb_leaves_to_visit > 0; nb_leaves_to_visit--) {
			struct rnode *current_leaf = shift(leaves_queue);
			geo_visit_leaf(current_leaf, prob_node_has_children,
					leaves_queue);
		}
	}

	char *newick = to_newick(root);
	printf("%s\n", newick);
	free(newick);
	destroy_llist(leaves_queue);
}

/******************************************************************/
/* Time-limited model */

double reciprocal_exponential_CDF(double x, double k)
{
	return - (log(1 - x)/k);
}

double tlt_grow_leaf(struct rnode *leaf, double branch_termination_rate,
		double alt_random)
{
	double rn;
       	if (alt_random < 0) {
		rn = rand();
	       	rn /= RAND_MAX;
		/* I remove 1/RAND_MAX so that I
		 * never get exactly 1.0 (which would yield Infinity) */
		rn -= 1 / RAND_MAX;
	}
       	else
	       rn = alt_random;

	/* Just an alias: keeps the formula below readable */
	double k = branch_termination_rate;
	double length = reciprocal_exponential_CDF(rn, k);

	/* The remaining time is the parent edge's length (just
	 * computed) minus the time threshold stored in the leaf's data
	 * pointer */
	fprintf (stderr, "leaf data: %g\n", *((double*) leaf->data));
	double remaining_time = (*((double*)leaf->data)) - length;
	/* Add remaining time if it's negative: this caps the branch at the
	 * time threshold */
	if (remaining_time < 0)
		length += remaining_time;

	char *length_s;
       	asprintf(&length_s, "%g", length);
	leaf->parent_edge->length_as_string = length_s;

	/* Return the remaining time so calling f() can take action based on
	 * whether there is time left or not */

	return remaining_time;
}

/* Creates a node with the specified time limit. */ 

struct rnode *create_child_with_time_limit(double time_limit)
{
	fprintf (stderr, "time limit: %g\n", time_limit);
	char *label;
	asprintf(&label, "n%d", running_number());
	struct rnode *kid = create_rnode(label);
	double *limit = malloc(sizeof(double));
	if (NULL == limit) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	*limit = time_limit;
	kid->data = limit;

	fprintf (stderr, "kid time limit: %g\n", *((double*) kid->data));
	return kid;
}

void time_limited_tree(double branch_termination_rate, double duration)
{
	/* create root */
	char *label;
	asprintf(&label, "n%d", running_number());
	struct rnode *root = create_rnode(label);

	/* This list remembers all children nodes created, for purposes of
	 * freeing */
	struct llist *all_children = create_llist();

	/* This list stores children who have yet to grow (and maybe have
	 * children of their own) */
	struct llist *leaves_queue = create_llist();
	struct rnode *kid;

	/* 1st child */
	kid = create_child_with_time_limit(duration);
	fprintf (stderr, "kid time limit: %g\n", *((double*) kid->data));
	link_p2c(root, kid, "");	/* length is determined below */
	append_element(leaves_queue, kid);
	append_element(all_children, kid);

	/* 2nd child */
	kid = create_child_with_time_limit(duration);
	link_p2c(root, kid, "");	
	append_element(leaves_queue, kid);
	append_element(all_children, kid);


	while (0 != leaves_queue->count) {
		struct rnode *current = shift(leaves_queue);
		double remaining_time = tlt_grow_leaf(current,
				branch_termination_rate, UNUSED); 
		if (remaining_time > 0) {
			kid = create_child_with_time_limit(remaining_time);
			link_p2c(current, kid, "");
			append_element(leaves_queue, kid);
			append_element(all_children, kid);

			kid = create_child_with_time_limit(remaining_time);
			link_p2c(current, kid, "");
			append_element(leaves_queue, kid);
			append_element(all_children, kid);
		} 
	}

	char *newick = to_newick(root);
	printf("%s\n", newick);

	/* free memory */
	free(newick);
	destroy_llist(leaves_queue);

	free(root);
	struct list_elem *elem;
	for (elem = all_children->head; NULL != elem; elem = elem->next) {
		kid = elem->data;
	       	free(kid->data);
		free(kid);
	}
	destroy_llist(all_children);
}
