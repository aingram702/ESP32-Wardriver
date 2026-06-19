// ===========================================================================
//  oui.h  —  Tiny built-in OUI (vendor) lookup table.
//
//  Maps the first 3 bytes of a MAC address (the IEEE "Organizationally Unique
//  Identifier") to a manufacturer name. This is a deliberately small, common
//  subset — the full IEEE registry is ~35k entries and won't fit comfortably.
//  Unknown prefixes simply report "Unknown". Locally-administered / random
//  MACs (common on modern phones) are detected separately.
// ===========================================================================
#pragma once
#include <Arduino.h>

struct OuiEntry { uint32_t prefix; const char* vendor; };

// prefix = (b0<<16)|(b1<<8)|b2
static const OuiEntry OUI_TABLE[] = {
    {0x000C29, "VMware"},        {0x005056, "VMware"},
    {0x001A11, "Google"},        {0x3C5AB4, "Google"},        {0xF4F5E8, "Google"},
    {0x0017AB, "Nintendo"},
    {0x001124, "Apple"},         {0x0017F2, "Apple"},         {0x002241, "Apple"},
    {0x3C0754, "Apple"},         {0xA4C361, "Apple"},         {0xF0DBF8, "Apple"},
    {0xDC2B2A, "Apple"},         {0x9803D8, "Apple"},
    {0x001E58, "D-Link"},        {0x1CBDB9, "D-Link"},        {0xC8BE19, "D-Link"},
    {0x000FB5, "Netgear"},       {0x2C3033, "Netgear"},       {0xA040A0, "Netgear"},
    {0x001CDF, "Belkin"},        {0x944452, "Belkin"},
    {0x000E08, "Cisco-Linksys"}, {0x001310, "Cisco"},         {0x00000C, "Cisco"},
    {0x001B0D, "Cisco"},         {0x000256, "Cisco"},         {0xF09E63, "Cisco"},
    {0x0018F8, "Cisco-Linksys"}, {0x002369, "Cisco-Linksys"},
    {0xB827EB, "Raspberry Pi"},  {0xDCA632, "Raspberry Pi"},  {0xE45F01, "Raspberry Pi"},
    {0x28CDC1, "Raspberry Pi"},  {0xD83ADD, "Raspberry Pi"},
    {0x7CDFA1, "Espressif"},     {0x240AC4, "Espressif"},     {0x3C6105, "Espressif"},
    {0x84F3EB, "Espressif"},     {0xA4CF12, "Espressif"},     {0xB4E62D, "Espressif"},
    {0xD8A01D, "Espressif"},     {0xEC94CB, "Espressif"},     {0x083AF2, "Espressif"},
    {0x001599, "Samsung"},       {0x0021D1, "Samsung"},       {0x5CF6DC, "Samsung"},
    {0x8425DB, "Samsung"},       {0xE8508B, "Samsung"},
    {0x001E2A, "Netgear"},       {0x00904C, "Epigram"},
    {0x18A6F7, "TP-Link"},       {0x50C7BF, "TP-Link"},       {0xA42BB0, "TP-Link"},
    {0xEC086B, "TP-Link"},       {0x6466B3, "TP-Link"},       {0xC006C3, "TP-Link"},
    {0x002586, "Aruba"},         {0x186472, "Aruba"},         {0x6CF37F, "Aruba"},
    {0x000B86, "Aruba"},         {0xD8C7C8, "Aruba"},
    {0x00037F, "Atheros"},       {0x001374, "Atheros"},
    {0x0024B2, "Ubiquiti"},      {0x44D9E7, "Ubiquiti"},      {0x788A20, "Ubiquiti"},
    {0xFCECDA, "Ubiquiti"},      {0x687251, "Ubiquiti"},      {0xE063DA, "Ubiquiti"},
    {0x245A4C, "Ruckus"},        {0xC4108A, "Ruckus"},        {0x2C5D93, "Ruckus"},
    {0x000508, "Fortinet"},      {0x0009F5, "Huawei"},        {0x00259E, "Huawei"},
    {0x4C5499, "Huawei"},        {0x80FB06, "Huawei"},        {0xACE215, "Huawei"},
    {0x001882, "Huawei"},        {0x086AC5, "Mitsumi"},
    {0x002347, "Zte"},           {0x9C2879, "Zte"},
    {0x0050F2, "Microsoft"},     {0x7C1E52, "Microsoft"},     {0x000D3A, "Microsoft"},
    {0xDC4A3E, "Hewlett-Packard"},{0x3024A9, "Hewlett-Packard"},
    {0x000423, "Intel"},         {0x001302, "Intel"},         {0x3CA9F4, "Intel"},
    {0x7CB27D, "Intel"},         {0xA0A8CD, "Intel"},         {0xE4A471, "Intel"},
    {0x00037A, "Taiyo"},         {0x001CB3, "Apple"},
    {0xC83A35, "Tenda"},         {0x0023CD, "Tenda"},
    {0x0024A5, "Buffalo"},       {0x10C37B, "Asus"},          {0x2C56DC, "Asus"},
    {0x382C4A, "Asus"},          {0x50465D, "Asus"},          {0xAC220B, "Asus"},
    {0x04D4C4, "Asus"},          {0x1C872C, "Asus"},
};

inline bool isLocallyAdministered(const uint8_t* mac) {
    // Bit 1 of the first octet set => locally administered (often a random MAC).
    return (mac[0] & 0x02) != 0;
}

inline const char* ouiLookup(const uint8_t* mac) {
    if (isLocallyAdministered(mac)) return "Randomized/Local";
    uint32_t key = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];
    for (const auto& e : OUI_TABLE) if (e.prefix == key) return e.vendor;
    return "Unknown";
}
