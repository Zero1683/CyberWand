import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('gui_server',Path(__file__).resolve().parents[2]/'studio/server.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ProtocolTest(unittest.TestCase):
    def test_status_and_bounded_stream(self):
        d=module.Device();d.parse('STATUS calibrated=1 IMU=OK samples=42 errors=0 train=-1 storage=1 capturing=0 drops=0')
        self.assertTrue(d.ready)
        for i in range(1000):d.parse(f'MOTION {i},1,2,3,0,0,1')
        self.assertEqual(len(d.motion),260)
        self.assertEqual(len(d.lines),1)
        d.parse('MOTION invalid');self.assertEqual(len(d.motion),260)
    def test_training_not_scored_and_expected_latched(self):
        d=module.Device();d.expected='left';d.parse('CAPTURE START mode=left');d.parse('REJECT capture bad=1')
        self.assertFalse(d.history)
        d.parse('TRAIN SAVED left 2/3');self.assertEqual(d.templates['left'],2)
        d.parse('CAPTURE START mode=recognize');d.expected='right';d.parse('RESULT left distance=0.1 margin=0.9')
        self.assertEqual(d.history[0]['expected'],'left')
    def test_command_validation(self):
        d=module.Device()
        with self.assertRaises(ValueError):d.send('erase_flash')
        with self.assertRaises(ValueError):d.send('train left')
    def test_calibration_and_features(self):
        d=module.Device();d.parse('READY bias_dps=1,2,3');self.assertTrue(d.ready)
        d.parse('CALIBRATING');self.assertFalse(d.ready)
        d.parse('DATA BEGIN');d.parse('DATA 0,1,2,3,4,5,6');d.parse('DATA END')
        self.assertEqual(d.last_dump,[[0,1,2,3,4,5,6]])
    def test_training_complete_stays_training_and_cancel_clears_capture(self):
        d=module.Device();d.parse('TRAIN left hold SW4');d.parse('CAPTURE START mode=left trigger=button')
        d.parse('CAPTURE CANCEL mode_switch');self.assertFalse(d.capturing)
        d.parse('TRAIN COMPLETE select next label or recognize');self.assertEqual(d.mode,'left')
        d.parse('HEALTH ready=1 train=-1 capturing=0');self.assertEqual(d.mode,'recognize')
    def test_custom_labels_and_one_shot_state(self):
        d=module.Device();d.parse('LABEL {"id":"custom6","name":"荧光闪烁"}')
        self.assertEqual(d.names['custom6'],'荧光闪烁')
        d.parse('TRAIN custom6 automatic one-shot countdown=3')
        d.parse('RECORD phase=armed');self.assertEqual(d.health['phase'],'armed')
        d.parse('CAPTURE START mode=custom6 trigger=auto');d.parse('TRAIN SAVED custom6 1/3')
        d.parse('CAPTURE END duration=1000');d.parse('RECORD phase=review')
        self.assertFalse(d.capturing);self.assertFalse(d.history)
        self.assertEqual(d.templates['custom6'],1)
        d.parse('HEALTH ready=1 train=11 capturing=0 phase=review');self.assertEqual(d.mode,'custom6')
        d.parse('MODE recognize AUTO');d.parse('CAPTURE START mode=recognize trigger=auto')
        d.parse('RESULT custom6 distance=0.3 margin=0.4');self.assertEqual(d.history[0]['label'],'custom6')
    def test_transport_status_has_no_credentials(self):
        d=module.Device();d.parse('LINK {"mode":"ble","connected":true,"device":"MOZHANG-123456"}')
        self.assertEqual(d.snapshot()['link']['mode'],'ble')
        self.assertNotIn('password',d.snapshot()['link'])
if __name__=='__main__':unittest.main()
