// String handling functions from "https://github.com/yshalabi/covert-channel-tutorial"

/*
 * Convert a given ASCII string to a binary string.
 * From:
 * https://stackoverflow.com/questions/41384262/convert-string-to-binary-in-c
 */
bool* string_to_binary(uint8_t *s,int num_bytes, bool* binary) 
{
  if (s == NULL) return 0; /* no input string */

  size_t len = num_bytes; 

  // Each char is one byte (8 bits) and + 1 at the end for null terminator   
  for (size_t i = 0; i < len; ++i) { 
    uint8_t ch = s[i]; 
    for (int j = 7; j >= 0; --j) { 
      binary[i*8 + 7-j] = ch & (1 << j);
    } 
  } 

   return binary; 
 } 

/*
 * Convert 8 bit data stream into character and return
 */
uint8_t *conv_char(bool *data, int size, uint8_t *msg)
{
  for (int i = 0; i < size; i++) {
    char tmp[8];
    int k = 0;

    for (int j = i * 8; j < ((i + 1) * 8); j++) {
      if(data[j])
        tmp[k++] = '1';
      else
        tmp[k++] = '0';  	
    }

    char tm = strtol(tmp, 0, 2);
    msg[i] = (uint8_t)tm;
  }

  // msg[size] = '\0';
  return msg;
}

void print_bool_array(bool* array, int num_entries){  
  for(int i =0; i<num_entries; i++){
    printf("%d",array[i]);
    if(i%8 == 7)
      printf(" ");    
  }
  printf("\n");
}
