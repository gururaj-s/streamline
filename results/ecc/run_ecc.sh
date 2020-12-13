#!/usr/bin/zsh
echo ""> receiver_out.log
echo "">sender_out.log
echo "numbits bps ber ber10 ber01 ber1bit bermultibit" > bitrate_ECC_results.txt;
echo "numbits bps ber | ber10 ber01 | ber1bit bermultibit"


## Run the experiment for payload size in bits of 200,000 to 1 billion.
for i in 200000 500000 1000000 2000000 5000000  10000000 20000000 50000000 100000000 200000000 500000000 1000000000 ; do

    ## Run the experiment 5 times.
    out_a=`sudo ../../bin/receiver_ECC.o -n $i &; sudo ../../bin/sender_ECC.o -n $i >>sender_out.log 2>&1 ;`;
    echo "----------" >> sender_out.log ; 
    out_b=`sudo ../../bin/receiver_ECC.o -n $i &; sudo ../../bin/sender_ECC.o -n $i >>sender_out.log 2>&1 ;`;
    echo "----------" >> sender_out.log ; 
    out_c=`sudo ../../bin/receiver_ECC.o -n $i &; sudo ../../bin/sender_ECC.o -n $i >>sender_out.log 2>&1 ;`;
    echo "----------" >> sender_out.log ; 
    out_d=`sudo ../../bin/receiver_ECC.o -n $i &; sudo ../../bin/sender_ECC.o -n $i >>sender_out.log 2>&1 ;`;
    echo "----------" >> sender_out.log ; 
    out_e=`sudo ../../bin/receiver_ECC.o -n $i &; sudo ../../bin/sender_ECC.o -n $i >>sender_out.log 2>&1 ;`;

    ## Get the string corresponding to the bit-period achieved in each run
    a=`echo "$out_a" | grep "Bit Period" | tail -n1 | awk '{print $8,$10,$12,$13}' | sed 's/FinalCorrectSamples=//' | sed 's/\%//g' | sed 's/Tx1_to_0_errors=//' | sed 's/Tx0_to_1_errors=//' | sed 's/,//'`
    b=`echo "$out_b" | grep "Bit Period" | tail -n1 | awk '{print $8,$10,$12,$13}' | sed 's/FinalCorrectSamples=//' | sed 's/\%//g' | sed 's/Tx1_to_0_errors=//' | sed 's/Tx0_to_1_errors=//' | sed 's/,//'`
    c=`echo "$out_c" | grep "Bit Period" | tail -n1 | awk '{print $8,$10,$12,$13}' | sed 's/FinalCorrectSamples=//' | sed 's/\%//g' | sed 's/Tx1_to_0_errors=//' | sed 's/Tx0_to_1_errors=//' | sed 's/,//'`
    d=`echo "$out_d" | grep "Bit Period" | tail -n1 | awk '{print $8,$10,$12,$13}' | sed 's/FinalCorrectSamples=//' | sed 's/\%//g' | sed 's/Tx1_to_0_errors=//' | sed 's/Tx0_to_1_errors=//' | sed 's/,//'`
    e=`echo "$out_e" | grep "Bit Period" | tail -n1 | awk '{print $8,$10,$12,$13}' | sed 's/FinalCorrectSamples=//' | sed 's/\%//g' | sed 's/Tx1_to_0_errors=//' | sed 's/Tx0_to_1_errors=//' | sed 's/,//'`

    ## Get the bit-error-rate achieved in each run
    a1=`echo "$out_a" | grep "Bit-Error" | awk '{print $12,$13}' | sed 's/\%//g'`
    b1=`echo "$out_b" | grep "Bit-Error" | awk '{print $12,$13}' | sed 's/\%//g'`
    c1=`echo "$out_c" | grep "Bit-Error" | awk '{print $12,$13}' | sed 's/\%//g'`
    d1=`echo "$out_d" | grep "Bit-Error" | awk '{print $12,$13}' | sed 's/\%//g'`
    e1=`echo "$out_e" | grep "Bit-Error" | awk '{print $12,$13}' | sed 's/\%//g'`

    ## Get the bit-period achieved in each run
    bps_a=`echo $a | awk '{print $1}'`; bcr_a=`echo $a | awk '{print $2}'`; ber10_a=`echo $a | awk '{print $3}'`; ber01_a=`echo $a | awk '{print $4}'`;  ber1bit_a=`echo $a1 | awk '{print $1}'`; bermultibit_a=`echo $a1 | awk '{print $2}'`; 
    bps_b=`echo $b | awk '{print $1}'`; bcr_b=`echo $b | awk '{print $2}'`; ber10_b=`echo $b | awk '{print $3}'`; ber01_b=`echo $b | awk '{print $4}'`;  ber1bit_b=`echo $b1 | awk '{print $1}'`; bermultibit_b=`echo $b1 | awk '{print $2}'`; 
    bps_c=`echo $c | awk '{print $1}'`; bcr_c=`echo $c | awk '{print $2}'`; ber10_c=`echo $c | awk '{print $3}'`; ber01_c=`echo $c | awk '{print $4}'`;  ber1bit_c=`echo $c1 | awk '{print $1}'`; bermultibit_c=`echo $c1 | awk '{print $2}'`; 
    bps_d=`echo $d | awk '{print $1}'`; bcr_d=`echo $d | awk '{print $2}'`; ber10_d=`echo $d | awk '{print $3}'`; ber01_d=`echo $d | awk '{print $4}'`;  ber1bit_d=`echo $d1 | awk '{print $1}'`; bermultibit_d=`echo $d1 | awk '{print $2}'`; 
    bps_e=`echo $e | awk '{print $1}'`; bcr_e=`echo $e | awk '{print $2}'`; ber10_e=`echo $e | awk '{print $3}'`; ber01_e=`echo $e | awk '{print $4}'`;  ber1bit_e=`echo $e1 | awk '{print $1}'`; bermultibit_e=`echo $e1 | awk '{print $2}'`; 

    ## Average the different metrics across 5 runs
    ## bps - bit-rate in bits per second
    bps=$[(bps_a+bps_b+bps_c+bps_d+bps_e)/5];
    ## ber - bit-error-rate (percentage of bits received erroneously)
    ber=$[100-(bcr_a+bcr_b+bcr_c+bcr_d+bcr_e)/5];
    ## ber10 - percentage of bits where sent 1 but received 0;  ber01 - percentage of bits where sent 0  but received 1.
    ber10=$[(ber10_a+ber10_b+ber10_c+ber10_d+ber10_e)/5]; ber01=$[(ber01_a+ber01_b+ber01_c+ber01_d+ber01_e)/5];
    ## ber1bit - percentage of 8-byte packets with 1-bit error; bermultibit - percentage of 8-byte packets with multibit errors.
    ber1bit=$[(ber1bit_a+ber1bit_b+ber1bit_c+ber1bit_d+ber1bit_e)/5]; bermultibit=$[(bermultibit_a+bermultibit_b+bermultibit_c+bermultibit_d+bermultibit_e)/5];

    ## Print the bitrate and error-rate results
    printf "%d  %d  %0.2f%%  %0.2f%%  %0.2f%%  %0.2f%%  %0.2f%%\n" $i $bps $ber $ber10 $ber01 $ber1bit $bermultibit >> bitrate_ECC_results.txt;
    printf "%d  %d  %0.2f%% | %0.2f%%  %0.2f%% | %0.2f%%  %0.2f%%\n" $i $bps $ber $ber10 $ber01 $ber1bit $bermultibit 
    echo "$out_a" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
    echo "$out_b" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
    echo "$out_c" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
    echo "$out_d" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
    echo "$out_e" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
    echo "----------------------------------------------" >> receiver_out.log
done
