Final Project Parallel and Distributed Systems: Paradigms and Models (2018-19)

# Autonomic_Farm_Pattern
The goal is to provide a farm pattern ensuring (best effort) a given service time leveraging on dynamic
variation of the parallelism degree. The farm is instantiated and run by providing:
* I. A collection of input tasks to be computed (of type Tin)

* II. A function<Tout(Tin)> computing the single task

* III. An expected service time TS goal

* IV. An initial parallelism degree n w

During farm execution, autonomic farm management should increase or decrease the parallelism degree
in such a way its service time is as close as possible to the expected service time TS goal .
The pattern should be tested providing a collection of tasks such that the tasks in the initial, central and
final part all require a different average time to be computed (e.g. 4L in the first part, L in the second part
and 8L in the third part) and the task collection execution time is considerably longer than the time needed
to reconfigure the farm.
Free choice pattern
Students may propose any pattern of interest to be implemented, but the pattern must be agreed with
the professor. After agreeing the pattern subject the work to be performed is the one listed for the two
assigned patterns.
