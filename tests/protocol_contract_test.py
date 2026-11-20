"""Schema examples and cross-language rule/security contract checks (no hardware)."""
import json
import pathlib
import unittest
import jsonschema

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Contract(unittest.TestCase):
    def test_fixtures(self):
        base = ROOT / 'protocol/v1'
        fixtures = json.loads((base / 'fixtures.json').read_text())
        request = json.loads((base / 'request.schema.json').read_text())
        response = json.loads((base / 'response.schema.json').read_text())
        for value in fixtures['requests']:
            jsonschema.validate(value, request)
        for value in fixtures['responses']:
            jsonschema.validate(value, response)
        for value in fixtures['invalid_requests']:
            with self.assertRaises(jsonschema.ValidationError):
                jsonschema.validate(value, request)

    def test_named_timezone_rules_match(self):
        schema = json.loads((ROOT / 'protocol/v1/request.schema.json').read_text())
        rules = schema['allOf'][2]['then']['properties']['params']['properties']['timezone']['enum']
        dart = (ROOT / 'companion/desktop/lib/timezones.dart').read_text()
        firmware = (ROOT / 'main/timezone_rules.h').read_text()
        for rule in rules:
            self.assertIn(rule, dart)
            self.assertIn(rule, firmware)

    def test_single_usb_reader_and_no_legacy_secret_console(self):
        sources = list((ROOT / 'main').glob('*.c'))
        readers = [p.name for p in sources if 'usb_serial_jtag_read_bytes(' in p.read_text()]
        self.assertEqual(readers, ['companion_service.c'])
        self.assertNotIn('fgets(', (ROOT / 'main/network_service.c').read_text())
        self.assertNotIn('provisioning_task', (ROOT / 'main/network_service.c').read_text())

if __name__ == '__main__':
    unittest.main()
