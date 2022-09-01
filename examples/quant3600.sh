# # !/bin/bash
# # mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2
# # 这是所有平均值小于1e-10的supernode的下标
# #mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15
# # 这是所有平均值小于1e-9的supernode下标
# #mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13
# # 这是所有平均值小于1e-8的supernode下标
# # mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -Q  21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13
# # 这是所有平均值小于1e-7的supernode下标
# # mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13 26,8 25,10 26,14 26,16 26,15 26,9
# # 这是所有平均值小于1e-6的supernode下标
# # mpirun -n 4  ./run_pselinv_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13 26,8 25,10 26,14 26,16 26,15 26,9 26,17 26,10 23,16 24,7 23,17 25,11 24,16 24,17 19,5 23,18 22,1

# mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H129600.csc -file quantq29600_report_cuda -qidx 11
# mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below6 -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13 26,8 25,10 26,14 26,16 26,15 26,9 26,17 26,10 23,16 24,7 23,17 25,11 24,16 24,17 19,5 23,18 22,1
# mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below_cuda -Q 21,11 20,10 21,10 21,12 7,4 26,1 26,0 7,3 17,14 20,11 17,13 22,4 22,3 23,8 22,14 23,9 21,15 21,16 22,13 19,4 23,12 23,10 19,3 12,8 12,9 26,6 22,10 23,11 22,15 22,12 26,5 22,11 24,6 26,13 26,8 25,10 26,14 26,16 26,15 26,9 26,17 26,10 23,16 24,7 23,17 25,11 24,16 24,17 19,5 23,18 22,1

# mpirun -n 10 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below5 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,5 26,6 26,8 26,9 7,3 7,4 

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below4 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 25,11 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,17 26,5 26,6 26,8 26,9 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below4 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 25,11 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,17 26,5 26,6 26,8 26,9 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below4 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 25,11 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,17 26,5 26,6 26,8 26,9 7,3 7,4 
done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below5 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,5 26,6 26,8 26,9 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below5 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,5 26,6 26,8 26,9 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below5 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,16 23,17 23,8 23,9 24,16 24,17 24,6 24,7 25,10 26,0 26,1 26,10 26,13 26,14 26,15 26,16 26,5 26,6 26,8 26,9 7,3 7,4 
done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below6 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 24,6 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below6 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 24,6 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below6 -Q 12,8 12,9 17,13 17,14 19,3 19,4 19,5 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 24,6 26,0 26,1 26,5 26,6 7,3 7,4 

done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below7 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below7 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below7 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 
done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below8 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below8 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below8 -Q 12,8 12,9 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 21,16 22,10 22,11 22,12 22,13 22,14 22,15 22,3 22,4 23,10 23,11 23,12 23,8 23,9 26,0 26,1 26,5 26,6 7,3 7,4 

done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below9 -Q 12,8 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 22,10 22,13 22,14 22,3 22,4 23,8 23,9 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below9 -Q 12,8 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 22,10 22,13 22,14 22,3 22,4 23,8 23,9 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below9 -Q 12,8 17,13 17,14 19,3 19,4 20,10 20,11 21,10 21,11 21,12 21,15 22,10 22,13 22,14 22,3 22,4 23,8 23,9 26,0 26,1 7,3 7,4 

done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below10 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 22,3 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below10 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 22,3 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below10 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 22,3 26,0 26,1 7,3 7,4 

done

for((i=0;i < 10;i ++))
do
mpirun -n 1 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below11 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_normal  -H H3600.csc -file ./report/normal/below11 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 26,0 26,1 7,3 7,4 
mpirun -n 1 ./run_pselinv_linux_release_mix  -H H3600.csc -file ./report/mix/below11 -Q 17,13 17,14 20,10 20,11 21,10 21,11 21,12 26,0 26,1 7,3 7,4 

done

# for((i=0; i<10; i++))
# do
# mpirun -n 10 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below
# mpirun -n 10 ./run_pselinv_linux_release_v2.0  -H H3600.csc -file ./report/cuda/below
# done



# # for((i=0; i<=26; i++))
# # do
# # mpirun -n 4 ./run_pselinv_linux_release_v2.0 -H H3600.csc -Q 26,$i
# # mpirun -n 4 ./run_pselinv_linux_release_v2.0 -H H3600.csc -Q $i,0
# # done

# # mpirun -n 4  ./my_dispalySuperNode_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -delta 1e-6

# # mpirun -n 4  ./my_dispalySuperNode_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -delta 1e-7

# # mpirun -n 4  ./my_dispalySuperNode_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -delta 1e-8

# # mpirun -n 4  ./my_dispalySuperNode_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -delta 1e-9

# # mpirun -n 4  ./my_dispalySuperNode_linux_release_v2.0 -H H3600.csc -r 2 -c 2 -delta 1e-10