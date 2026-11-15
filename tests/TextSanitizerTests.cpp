#include "../src/utils/TextSanitizer.hpp"
#include <cassert>
#include <iostream>
int main(){using geobr::text::sanitizeMessage;assert(sanitizeMessage("   Ola    GeoBR   ",160)=="Ola GeoBR");assert(sanitizeMessage("<b>teste</b>",160)=="bteste/b");assert(sanitizeMessage("\n\t",160).empty());assert(sanitizeMessage("abc",2)=="ab");assert(sanitizeMessage("ação",3)=="açã");assert(sanitizeMessage("oi\nGeoBR",160)=="oi GeoBR");using geobr::text::truncateUtf8;assert(sanitizeMessage("a\x80" "b",160)=="ab");assert(sanitizeMessage("x\xC0\xAF" "y",160)=="xy");assert(sanitizeMessage("\xF8\x88\x80\x80\x80" "ok",160)=="ok");assert(truncateUtf8("\xE2\x82",5).empty());assert(truncateUtf8("\xF0\x9F\x98\x80!",1)=="\xF0\x9F\x98\x80");std::cout<<"TextSanitizer: 11 testes aprovados\n";}
