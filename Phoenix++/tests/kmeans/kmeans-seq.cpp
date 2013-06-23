#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Global variables
#define DEF_NUM_POINTS 100000
#define DEF_NUM_MEANS 100
#define DEF_DIM 3
#define DEF_GRID_SIZE 1000
#define false 0
#define true 1
int num_points, dim, num_means, grid_size, modified;

/** point structure
 *  An n-dimensional point
 */
struct point {
	int cluster, *dimension;

	point() {cluster = -1; dimension = NULL;}
	point(int *d, int c) {cluster = c; dimension = d;}

	/** get_sq_dist() 
	 *  Get the square distance between another point
	 */
	unsigned int get_sq_dist(struct point p) {
		unsigned int sum = 0;
		int i;
		
		for (i = 0; i < dim; i++) {
			int diff = (dimension[i] - p.dimension[i]);
			sum += diff * diff;
		}
		return sum;
	}
	/** generate_point()
	 *  Fill point with a random number
	 */
	void generate_point() {
		int i;
		for (i = 0; i < dim; i++)
			dimension[i] = rand() % grid_size;
	}
};

/** parse_args()
 *  Parse the command line arguments
 */
void parse_args(int argc, char **argv) {
	int c;
	extern char *optarg;

	num_points = DEF_NUM_POINTS;
	dim = DEF_DIM;
	num_means = DEF_NUM_MEANS;
	grid_size = DEF_GRID_SIZE;

	while ((c = getopt(argc, argv, "d:c:p:s:")) != EOF) {
		switch (c) {
			case 'd':
				dim = atoi(optarg);
				break;
			case 'c': 
				num_means = atoi(optarg);
				break;
			case 'p':
				num_points = atoi(optarg);
				break;
			case 's':
				grid_size = atoi(optarg);
				break;
			case '?':
				printf("Usage: %s -d <vector dimension> -c <num clusters> -p <num points> -s <grid size>\n", argv[0]);
				exit(1);
		}
	}

	printf("Dimension = %d\n", dim);
	printf("Number of clusters = %d\n", num_means);
	printf("Number of points = %d\n", num_points);
	printf("Size of each dimension = %d\n", grid_size);
}

/** find_clusters()
 *  Find the cluster that is most suitable for a given set of points
 */ 
void find_clusters(struct point *points, struct point *means) {
	int i, j;
	unsigned int min, dist;
	int min_idx;

	for (i = 0; i < num_points; i++) {
		min = points[i].get_sq_dist(means[0]);
		min_idx = 0;
		// Compute the distance from each mean
		for (j = 1; j < num_means; j++) {
			dist = points[i].get_sq_dist(means[j]);
			if (dist < min) {
				min = dist;
				min_idx = j;
			}
		}

		// Reassign the point to a new cluster
		if (points[i].cluster != min_idx) {
			points[i].cluster = min_idx;
			modified = true;
		}
	}
}
/** compute_means() 
 *  Calculate the means for the new clusters
 */
void compute_means(struct point *points, struct point *means) {
	int i, j, k, group_size;
	int *sum_data;
	struct point *sum;

	sum_data = (int *) malloc(sizeof(int) * dim);
	sum = new point;
	sum[0] = point(&sum_data[0], -1);

	for (i = 0; i < num_means; i++) {
		// Track the distance from every other point
		for (k = 0; k < dim; k++) 
			sum[0].dimension[k] = 0;
		group_size = 0;

		for (j = 0; j < num_points; j++) {
			// If the point belongs to the current mean's cluster
			if (points[j].cluster == i) {
				// Add each dimension to the sum				
				for (k = 0; k < dim; k++) 
					sum[0].dimension[k] += points[j].dimension[k];
				group_size++;
			}
		}

		for (j = 0; j < dim; j++) {
			// If other points are assigned to the mean
			if (group_size != 0) 
				means[i].dimension[j] = sum[0].dimension[j] / group_size;
		}
		
	}
		
	free(sum_data);
	delete sum;
}

/** print_ptdata()
 *  Helper function for debugging
 */
void print_ptdata(struct point *points, int n) {
	int i, j;
	for (i = 0; i < n; i++) {
		//printf("point %d: ", i);
		printf("\t ");
		for (j = 0; j < dim; j++)
			printf("%d ", points[i].dimension[j]);
		printf("\n");
	} 
}

/** main()
 *  K-means: group 'num_points' points into 'num_means' clusters
 *  through the iterative k-means algorithm.
 */
int main(int argc, char **argv) {
	printf("Sequential KMeans\n");
	struct point *points, *means;
	int *ptdata, *meandata;
	int i;

	// Parse the command line arguments
	parse_args(argc, argv);

	// Random seed
	srand(time(NULL));

	// Generate the random points
	printf("Seq. KMeans: Generating points...\n");
	ptdata = (int *) malloc(sizeof(int) * num_points * dim); 
	points = new point[num_points];
	for (i = 0; i < num_points; i++) {
		points[i] = point(&ptdata[i*dim], -1);
		points[i].generate_point();
	}
	
	// Generate the random means
	printf("Seq. KMeans: Generating means...\n");
	meandata = (int *) malloc(sizeof(int) * num_means * dim);
	means = new point[num_means];
	for (i = 0; i < num_means; i++) {
		means[i] = point(&meandata[i*dim], -1);
		means[i].generate_point();
	}

	printf("Seq. KMeans: Generating clusters...\n");

	// Flag for exiting the loop
	modified = true;

	printf("Seq. KMeans: Starting iterative algorithm...\n");
	while(modified) {
		modified = false;
		printf(".");

		find_clusters(points, means);
		compute_means(points, means);
	}

	printf("\n\nSeq. KMeans: Finished\nFinal Means:\n");
	print_ptdata(means, num_means);

	// Free allocated memory
	printf("Cleaning up...\n");
	free(ptdata);
	free(meandata);
	delete [] points;
	delete [] means;
	
	return 0;
}
