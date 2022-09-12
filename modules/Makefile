SUBDIRS = hello scull scullp scullc scullpg scullv complete faulty sleepy jit \
          short

all: subdirs

subdirs:
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done

clean:
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done
