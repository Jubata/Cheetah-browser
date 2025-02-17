// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/tools/transport_security_state_generator/input_file_parsers.h"
#include "net/tools/transport_security_state_generator/pinsets.h"
#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace transport_security_state {

namespace {

// Test that all values are correctly parsed from a valid JSON input.
TEST(InputFileParsersTest, ParseJSON) {
  std::string valid =
      "{"
      "  \"pinsets\": [{"
      "      \"name\": \"test\","
      "      \"static_spki_hashes\": [\"TestSPKI\"],"
      "      \"bad_static_spki_hashes\": [\"BadTestSPKI\"],"
      "      \"report_uri\": \"https://hpkp-log.example.com\""
      "  }],"
      "  \"entries\": ["
      "    {"
      "      \"name\": \"hsts.example.com\","
      "      \"mode\": \"force-https\", "
      "      \"include_subdomains\": true"
      "    }, {"
      "      \"name\": \"hsts-no-subdomains.example.com\","
      "      \"mode\": \"force-https\", "
      "      \"include_subdomains\": false"
      "    }, {"
      "      \"name\": \"hpkp.example.com\","
      "      \"pins\": \"thepinset\","
      "      \"include_subdomains_for_pinning\": true"
      "    }, {"
      "      \"name\": \"hpkp-no-subdomains.example.com\","
      "      \"pins\": \"thepinset2\", "
      "      \"include_subdomains_for_pinning\": false"
      "    }, {"
      "      \"name\": \"expect-ct.example.com\","
      "      \"expect_ct\": true,"
      "      \"expect_ct_report_uri\": \"https://expect-ct-log.example.com\""
      "    }, {"
      "      \"name\": \"expect-staple.example.com\","
      "      \"expect_staple\": true,"
      "      \"expect_staple_report_uri\": "
      "\"https://expect-staple-log.example.com\","
      "      \"include_subdomains_for_expect_staple\": true"
      "    }, {"
      "      \"name\": \"expect-staple-no-subdomains.example.com\","
      "      \"expect_staple\": true,"
      "      \"include_subdomains_for_expect_staple\": false"
      "    }"
      "  ]"
      "}";

  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  EXPECT_TRUE(ParseJSON(valid, &entries, &pinsets));

  ASSERT_EQ(1U, pinsets.size());
  PinsetMap::const_iterator pinset = pinsets.pinsets().find("test");
  ASSERT_NE(pinset, pinsets.pinsets().cend());
  EXPECT_EQ("test", pinset->second->name());
  EXPECT_EQ("https://hpkp-log.example.com", pinset->second->report_uri());

  ASSERT_EQ(1U, pinset->second->static_spki_hashes().size());
  EXPECT_EQ("TestSPKI", pinset->second->static_spki_hashes()[0]);

  ASSERT_EQ(1U, pinset->second->bad_static_spki_hashes().size());
  EXPECT_EQ("BadTestSPKI", pinset->second->bad_static_spki_hashes()[0]);

  ASSERT_EQ(7U, entries.size());
  TransportSecurityStateEntry* entry = entries[0].get();
  EXPECT_EQ("hsts.example.com", entry->hostname);
  EXPECT_TRUE(entry->force_https);
  EXPECT_TRUE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_FALSE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);

  entry = entries[1].get();
  EXPECT_EQ("hsts-no-subdomains.example.com", entry->hostname);
  EXPECT_TRUE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_FALSE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);

  entry = entries[2].get();
  EXPECT_EQ("hpkp.example.com", entry->hostname);
  EXPECT_FALSE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_TRUE(entry->hpkp_include_subdomains);
  EXPECT_EQ("thepinset", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_FALSE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);

  entry = entries[3].get();
  EXPECT_EQ("hpkp-no-subdomains.example.com", entry->hostname);
  EXPECT_FALSE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("thepinset2", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_FALSE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);

  entry = entries[4].get();
  EXPECT_EQ("expect-ct.example.com", entry->hostname);
  EXPECT_FALSE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("", entry->pinset);
  EXPECT_TRUE(entry->expect_ct);
  EXPECT_EQ("https://expect-ct-log.example.com", entry->expect_ct_report_uri);
  EXPECT_FALSE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);

  entry = entries[5].get();
  EXPECT_EQ("expect-staple.example.com", entry->hostname);
  EXPECT_FALSE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_TRUE(entry->expect_staple);
  EXPECT_TRUE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("https://expect-staple-log.example.com",
            entry->expect_staple_report_uri);

  entry = entries[6].get();
  EXPECT_EQ("expect-staple-no-subdomains.example.com", entry->hostname);
  EXPECT_FALSE(entry->force_https);
  EXPECT_FALSE(entry->include_subdomains);
  EXPECT_FALSE(entry->hpkp_include_subdomains);
  EXPECT_EQ("", entry->pinset);
  EXPECT_FALSE(entry->expect_ct);
  EXPECT_EQ("", entry->expect_ct_report_uri);
  EXPECT_TRUE(entry->expect_staple);
  EXPECT_FALSE(entry->expect_staple_include_subdomains);
  EXPECT_EQ("", entry->expect_staple_report_uri);
}

// Test that parsing valid JSON with missing keys fails.
TEST(InputFileParsersTest, ParseJSONInvalid) {
  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  std::string no_pinsets =
      "{"
      "  \"entries\": []"
      "}";

  EXPECT_FALSE(ParseJSON(no_pinsets, &entries, &pinsets));

  std::string no_entries =
      "{"
      "  \"pinsets\": []"
      "}";

  EXPECT_FALSE(ParseJSON(no_entries, &entries, &pinsets));

  std::string missing_hostname =
      "{"
      "  \"pinsets\": [],"
      "  \"entries\": ["
      "    {"
      "      \"mode\": \"force-https\""
      "    }"
      "  ]"
      "}";

  EXPECT_FALSE(ParseJSON(missing_hostname, &entries, &pinsets));
}

// Test that parsing valid JSON with an invalid (HPKP) pinset fails.
TEST(InputFileParsersTest, ParseJSONInvalidPinset) {
  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  std::string missing_pinset_name =
      "{"
      "  \"pinsets\": [{"
      "      \"static_spki_hashes\": [\"TestSPKI\"],"
      "      \"bad_static_spki_hashes\": [\"BadTestSPKI\"],"
      "      \"report_uri\": \"https://hpkp-log.example.com\""
      "  }],"
      "  \"entries\": []"
      "}";

  EXPECT_FALSE(ParseJSON(missing_pinset_name, &entries, &pinsets));
}

// Test that parsing valid JSON containing an entry with an invalid mode fails.
TEST(InputFileParsersTest, ParseJSONInvalidMode) {
  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  std::string invalid_mode =
      "{"
      "  \"pinsets\": [],"
      "  \"entries\": ["
      "    {"
      "      \"name\": \"preloaded.test\","
      "      \"mode\": \"something-invalid\""
      "    }"
      "  ]"
      "}";

  EXPECT_FALSE(ParseJSON(invalid_mode, &entries, &pinsets));
}

// Test that parsing valid JSON containing an entry with an unknown field fails.
TEST(InputFileParsersTest, ParseJSONUnkownField) {
  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  std::string unknown_field =
      "{"
      "  \"pinsets\": [],"
      "  \"entries\": ["
      "    {"
      "      \"name\": \"preloaded.test\","
      "      \"unknown_key\": \"value\""
      "    }"
      "  ]"
      "}";

  EXPECT_FALSE(ParseJSON(unknown_field, &entries, &pinsets));
}

// Test parsing of all 3 SPKI formats.
TEST(InputFileParsersTest, ParseCertificatesFile) {
  std::string valid =
      "# This line should ignored. The rest should result in 3 pins.\n"
      "TestPublicKey1\n"
      "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n"
      "\n"
      "TestPublicKey2\n"
      "-----BEGIN PUBLIC KEY-----\n"
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAujzwcb5bJuC/A/Y9izGl\n"
      "LlA3fnKGbeyn53BdVznJN4fQwU82WKVYdqt8d/1ZDRdYyhGrTgXJeCURe9VSJyX1\n"
      "X2a5EApSFsopP8Yjy0Rl6dNOLO84KCW9dPmfHC3uP0ac4hnHT5dUr05YvhJmHCkf\n"
      "as6v/aEgpPLDhRF6UruSUh+gIpUg/F3+vlD99HLfbloukoDtQyxW+86s9sO7RQ00\n"
      "pd79VOoa/v09FvoS7MFgnBBOtvBQLOXjEH7/qBsnrXFtHBeOtxSLar/FL3OhVXuh\n"
      "dUTRyc1Mg0ECtz8zHZugW+LleIm5Bf5Yr0bN1O/HfDPCkDaCldcm6xohEHn9pBaW\n"
      "+wIDAQAB\n"
      "-----END PUBLIC KEY-----\n"
      "\n"
      "# The 'Chromium' prefix is required here.\n"
      "ChromiumTestCertificate3\n"
      "-----BEGIN CERTIFICATE-----\n"
      "MIIDeTCCAmGgAwIBAgIJAMRHXuiAgufAMA0GCSqGSIb3DQEBCwUAMFMxETAPBgNV\n"
      "BAMMCENocm9taXVtMR4wHAYDVQQKDBVUaGUgQ2hyb21pdW0gUHJvamVjdHMxETAP\n"
      "BgNVBAsMCFNlY3VyaXR5MQswCQYDVQQGEwJVUzAeFw0xNzAyMDExOTAyMzFaFw0x\n"
      "ODAyMDExOTAyMzFaMFMxETAPBgNVBAMMCENocm9taXVtMR4wHAYDVQQKDBVUaGUg\n"
      "Q2hyb21pdW0gUHJvamVjdHMxETAPBgNVBAsMCFNlY3VyaXR5MQswCQYDVQQGEwJV\n"
      "UzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALtggpf5vNVsmQrJKTQe\n"
      "ynTeOzVOyROGDugGtR+Cri8WlNg1UAlIyYIS8txZ4oCknsT8gs3TFfu0wxmWNxx5\n"
      "4oLGy2BQOHH00dgBAsKgqX//mY4mH5AZ85UFYni1hj9aszIJMIBWtgbNGVkppW65\n"
      "8maF1KVdHmxXMvtKxn/9UsusH/A0ng5UJDYBPISQMv0XqIlv0wdVTIVWIcQhOjWz\n"
      "MGwFDSjxS1WgEnPgd4Qi7MYaDbUTsXGtWba83vZJ8CQzjLumSJJCnz2aquGmraX0\n"
      "J0joUjB4fuYL8xrbDqnFmADvozMMVkZ4843w8ikvJkM8nWoIXexVvirfXDoqtdUo\n"
      "YOcCAwEAAaNQME4wHQYDVR0OBBYEFGJ6O/oLtzpb4OWvrEFxieYb1JbsMB8GA1Ud\n"
      "IwQYMBaAFGJ6O/oLtzpb4OWvrEFxieYb1JbsMAwGA1UdEwQFMAMBAf8wDQYJKoZI\n"
      "hvcNAQELBQADggEBAFpt9jlBT6OsfKFAJZnmExbW8JlsqXOJAaR+nD1XOnp6o+DM\n"
      "NIguj9+wJOW34OM+2Om0n+KMYbDER0p4g3gxoaDblu7otgnC0OTOnx3DPUYab0jr\n"
      "uT6O4C3/nfWW5sl3Ni3Y99dmdcqKcmYkHsr7uADLPWsjb+sfUrQQfHHnPwzyUz/A\n"
      "w4rSJ0wxnLOmjk5F5YHMLkNpPrzFA1mFyGIau7THsRIr3B632MLNcOlNR21nOc7i\n"
      "eB4u+OzpcZXuiQg3bqrNp6Xb70OIW1rfNEiCpps4UZyRnZ/nrzByxeHH5zPWWZk9\n"
      "nZtxI+65PFOekOjBpbnRC8v1CfOmUSVKIqWaPys=\n"
      "-----END CERTIFICATE-----";

  Pinsets pinsets;
  EXPECT_TRUE(ParseCertificatesFile(valid, &pinsets));
  EXPECT_EQ(3U, pinsets.spki_size());

  const SPKIHashMap& hashes = pinsets.spki_hashes();
  EXPECT_NE(hashes.cend(), hashes.find("TestPublicKey1"));
  EXPECT_NE(hashes.cend(), hashes.find("TestPublicKey2"));
  EXPECT_NE(hashes.cend(), hashes.find("ChromiumTestCertificate3"));
}

TEST(InputFileParsersTest, ParseCertificatesFileInvalid) {
  Pinsets pinsets;

  std::string invalid =
      "TestName\n"
      "unexpected";
  EXPECT_FALSE(ParseCertificatesFile(invalid, &pinsets));
}

// Test that parsing invalid certificate names fails.
TEST(InputFileParsersTest, ParseCertificatesFileInvalidName) {
  Pinsets pinsets;

  std::string invalid_name_small_character =
      "startsWithSmallLetter\n"
      "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n";
  EXPECT_FALSE(ParseCertificatesFile(invalid_name_small_character, &pinsets));

  std::string invalid_name_invalid_characters =
      "Invalid-Characters-In-Name\n"
      "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n";
  EXPECT_FALSE(
      ParseCertificatesFile(invalid_name_invalid_characters, &pinsets));

  std::string invalid_name_number =
      "1InvalidName\n"
      "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n";
  EXPECT_FALSE(ParseCertificatesFile(invalid_name_number, &pinsets));

  std::string invalid_name_space =
      "Invalid Name\n"
      "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n";
  EXPECT_FALSE(ParseCertificatesFile(invalid_name_space, &pinsets));
}

// Test that parsing of a certificate with an incomplete or incorrect name
// fails.
TEST(InputFileParsersTest, ParseCertificatesFileInvalidCertificateName) {
  Pinsets pinsets;
  std::string certificate =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIDIzCCAgugAwIBAgIJALs84KlxWh4GMA0GCSqGSIb3DQEBCwUAMCgxGTAXBgNV\n"
      "BAoMEENocm9taXVtIENsYXNzIDMxCzAJBgNVBAsMAkcxMB4XDTE3MDIwMTE5NTUw\n"
      "NVoXDTE4MDIwMTE5NTUwNVowKDEZMBcGA1UECgwQQ2hyb21pdW0gQ2xhc3MgMzEL\n"
      "MAkGA1UECwwCRzEwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDkolrR\n"
      "7gCPm22Cc9psS2Jh1mksVneee5ntEezZ2gEU20y9Z9URBReo8SFvaZcgKkAkca1v\n"
      "552YIG+FBO/u8njxzlHXvuVJ5x2geciqqR4TRhA4jO1ndrNW6nlJfOoYueWbdym3\n"
      "8zwugoULoCtyLyzdiMI5g8iVBQHDh8+K3TZIHar3HS49TjX5u5nv4igO4RfDcFUa\n"
      "h8g+6x5nWoFF8oa3FG0YTN+q6iI1i2JHmj/q03fVPv3WLPGJ3JADau9gO1Lw1/qf\n"
      "R/N3l4MVtjDFFGYzclfqW2UmL6zRirEV0GF2gwSBAGVX3WWhpOcM8rFIWYkZCsI5\n"
      "iUdtwFNBfcKS9sNpAgMBAAGjUDBOMB0GA1UdDgQWBBTm4VJfibducqwb9h4XELn3\n"
      "p6zLVzAfBgNVHSMEGDAWgBTm4VJfibducqwb9h4XELn3p6zLVzAMBgNVHRMEBTAD\n"
      "AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQApTm40RfsZG20IIgWJ62pZ2end/lvaneTh\n"
      "MZSgFnoTRjKkd/5dh22YyKPw9PnpIuiyi85L36COreqZUvbxqRQnpL1oSCRlLBJQ\n"
      "2LcGlF0j0Opa+SY2VWup4XjnYF8CvwMl4obNpSuywTFmkXCRxzN23tn8whNHvWHM\n"
      "BQ7abw8X1KY02uPbHucrpou6KXkKkhyhfML8OD8IRkSM56K6YyedqV97cmEdW0Ie\n"
      "LlpFJQVX13bmojtSNI1zaiCiEenn5xLa/dAlyFT18Mq6y8plioBinVWFYd0qcRoA\n"
      "E2j3m+jTVIv3CZ+ivGxggZQ8ZYN8FJ/iTW3pXGojogHh0NRJJ8dM\n"
      "-----END CERTIFICATE-----";

  std::string missing_prefix = "Class3_G1_Test\n" + certificate;
  EXPECT_FALSE(ParseCertificatesFile(missing_prefix, &pinsets));

  std::string missing_class = "Chromium_G1_Test\n" + certificate;
  EXPECT_FALSE(ParseCertificatesFile(missing_class, &pinsets));

  std::string missing_number = "Chromium_Class3_Test\n" + certificate;
  EXPECT_FALSE(ParseCertificatesFile(missing_number, &pinsets));

  std::string valid = "Chromium_Class3_G1_Test\n" + certificate;
  EXPECT_TRUE(ParseCertificatesFile(valid, &pinsets));
}

}  // namespace

}  // namespace transport_security_state

}  // namespace net
