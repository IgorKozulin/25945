TASKS = t1 t2 t3 t4 t5 t6 t7 t8 t9

all clean:
	@for task in $(TASKS); do $(MAKE) -C $$task $@ || exit $$?; done

test: all
	python3 test_tasks.py

.PHONY: all clean test
