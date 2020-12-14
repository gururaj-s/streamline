#Run the base attack (Fig-9, Table-2)
echo "Running base attack (Fig-9, Table-2)"
cd results/base; ./run_base.sh

#Run the attack with ECC (Table-3)
echo " "
echo "Running attack with ECC (Table-3)"
cd ../../results/ecc; ./run_ecc.sh

#Run sensitivity to varying shared array sizes (Table-3)
echo " "
echo "Running sensitivity to varying array sizes (Table-4)"
cd ../../results/array_sz; ./run_array_sz.sh

#Run sensitivity to varying synchronization periods (Table-4)
echo " "
echo "Running sensitivity to varying synchronization periods (Table-4)"
cd ../../results/sync_period; ./run_sync_period.sh
