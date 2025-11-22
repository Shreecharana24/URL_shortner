# URL Shortener in C

**Overview**  
This project implements a URL Shortener in C using two hash tables: one for mapping short codes to long URLs, and another for mapping long URLs to short codes. The system supports O(1) average-time insertion, lookup, and deletion operations.

**Key Features**  
- Two-way mapping using separate hash tables.
- Base62 short code generation with fixed 7-character codes.
- Scrambled ID generation to avoid predictable patterns.
- Collision handling using separate chaining.
- Supports long URLs up to 1024 characters.
- Clean dynamic memory management.

**Commands**  
gen <long_url>   - Generate a short code for a URL.  
get <short_code> - Retrieve original URL from short code.  
del <short_code> - Delete a mapping.  
list             - Display all mappings.  
count            - Count non-empty buckets.  
exit             - Exit the program. 

**Build Instructions**  
Compile using:
  gcc main.c -o shortener.exe
Run using:
  ./shortener.exe
