#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------

#define MAX_ENTRIES 124

struct dm_btree_info {
};

struct btree_header {
	unsigned max_entries;
	unsigned nr_entries;
};

struct btree_node {
	struct btree_header header;
	unsigned entries[MAX_ENTRIES];
};

static void shift(struct btree_node *left, struct btree_node *right, int count)
{
	if (count < 0) {
		count = -count;

		assert(right->header.nr_entries >= count);
		assert(left->header.nr_entries + count <= MAX_ENTRIES);

		memcpy(left->entries + left->header.nr_entries, right->entries, sizeof(unsigned) * count);
		memmove(right->entries, right->entries + count, sizeof(unsigned) * (right->header.nr_entries - count));
		left->header.nr_entries += count;
		right->header.nr_entries -= count;

	} else {
		assert(left->header.nr_entries >= count);
		assert(right->header.nr_entries + count <= MAX_ENTRIES);

		memmove(right->entries + count, right->entries, sizeof(unsigned) * right->header.nr_entries);
		memcpy(right->entries, left->entries + left->header.nr_entries - count, sizeof(unsigned) * count);

		left->header.nr_entries -= count;
		right->header.nr_entries += count;
	}
}

struct child {
};

uint32_t le32_to_cpu(uint32_t n)
{
	return n;
}

#define BUG_ON(expr) assert(!(expr))

//----------------------------------------------------------------

#if 1
// New code
static void redistribute3(struct dm_btree_info *info, struct btree_node *parent,
			  struct child *l, struct child *c, struct child *r,
			  struct btree_node *left, struct btree_node *center, struct btree_node *right,
			  uint32_t nr_left, uint32_t nr_center, uint32_t nr_right)
{
	int s;
	uint32_t max_entries = le32_to_cpu(left->header.max_entries);
	unsigned total = nr_left + nr_center + nr_right;
	unsigned target_right = total / 3;
	unsigned remainder = (target_right * 3) != total;
	unsigned target_left = target_right + remainder;

	BUG_ON(target_left > max_entries);
	BUG_ON(target_right > max_entries);

	if (nr_left < nr_right) {
		s = nr_left - target_left;

		if (s < 0 && nr_center < -s) {
			/* not enough in central node */
			shift(left, center, -nr_center);
			s += nr_center;
			shift(left, right, s);
			nr_right += s;
		} else
			shift(left, center, s);

		shift(center, right, target_right - nr_right);

	} else {
		s = target_right - nr_right;
		if (s > 0 && nr_center < s) {
			/* not enough in central node */
			shift(center, right, nr_center);
			s -= nr_center;
			shift(left, right, s);
			nr_left -= s;
		} else
			shift(center, right, s);

		shift(left, center, nr_left - target_left);
	}

#if 0
	*key_ptr(parent, c->index) = center->keys[0];
	*key_ptr(parent, r->index) = right->keys[0];
#endif
}
#else
// Old coed
static void redistribute3(struct dm_btree_info *info, struct btree_node *parent,
			  struct child *l, struct child *c, struct child *r,
			  struct btree_node *left, struct btree_node *center, struct btree_node *right,
			  uint32_t nr_left, uint32_t nr_center, uint32_t nr_right)
{
	int s;
	uint32_t max_entries = le32_to_cpu(left->header.max_entries);
	unsigned target = (nr_left + nr_center + nr_right) / 3;
	BUG_ON(target > max_entries);

	if (nr_left < nr_right) {
		s = nr_left - target;

		if (s < 0 && nr_center < -s) {
			/* not enough in central node */
			shift(left, center, -nr_center);
			s += nr_center;
			shift(left, right, s);
			nr_right += s;
		} else
			shift(left, center, s);

		shift(center, right, target - nr_right);

	} else {
		s = target - nr_right;
		if (s > 0 && nr_center < s) {
			/* not enough in central node */
			shift(center, right, nr_center);
			s -= nr_center;
			shift(left, right, s);
			nr_left -= s;
		} else
			shift(center, right, s);

		shift(left, center, nr_left - target);
	}

#if 0
	*key_ptr(parent, c->index) = center->keys[0];
	*key_ptr(parent, r->index) = right->keys[0];
#endif
}

#endif

//----------------------------------------------------------------

static void prepare_node(struct btree_node *n, unsigned begin, unsigned count)
{
	unsigned i;

	assert(count <= MAX_ENTRIES);
	for (i = 0; i < count; i++)
		n->entries[i] = begin + i;
	n->header.nr_entries = count;
}

static void check_node(struct btree_node *n, unsigned begin, const char *label)
{
	unsigned i;
	for (i = 0; i < n->header.nr_entries; i++)
		if (n->entries[i] != begin + i) {
			abort();
		}
}

static int within_one(struct btree_node *left,
		      struct btree_node *right)
{
	unsigned l = left->header.nr_entries;
	unsigned r = right->header.nr_entries;
	if (l < r)
		return (r - l) <= 1;
	else
		return (l - r) <= 1;
}

static void check_nodes(struct btree_node *left,
			struct btree_node *center,
			struct btree_node *right,
			unsigned total_entries)
{
	assert(left->header.nr_entries + center->header.nr_entries + right->header.nr_entries == total_entries);

	assert(within_one(left, center));
	assert(within_one(center, right));
	assert(within_one(left, right));

	check_node(left, 0, "left");
	check_node(center, left->header.nr_entries, "center");
	check_node(right, left->header.nr_entries + center->header.nr_entries, "right");
}

int main(int argc, char **argv)
{
	struct dm_btree_info info;
	struct child l, c, r;
	struct btree_node parent;
	struct btree_node left, center, right;

	unsigned max_entries = MAX_ENTRIES;
	unsigned nr_left, nr_center, nr_right;

	left.header.max_entries = center.header.max_entries = right.header.max_entries = max_entries;

	for (nr_left = 0; nr_left <= max_entries; nr_left++) {
		for (nr_center = 0; nr_center <= max_entries; nr_center++) {
			for (nr_right = 0; nr_right <= max_entries; nr_right++) {
				prepare_node(&left, 0, nr_left);
				prepare_node(&center, nr_left, nr_center);
				prepare_node(&right, nr_left + nr_center, nr_right);

				redistribute3(&info,
					      &parent, &l, &c, &r,
					      &left, &center, &right,
					      nr_left, nr_center, nr_right);

				check_nodes(&left, &center, &right,
					    nr_left + nr_center + nr_right);
			}
		}
	}

	return 0;
}

//----------------------------------------------------------------
