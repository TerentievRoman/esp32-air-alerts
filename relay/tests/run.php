<?php
// Тести ретранслятора без фреймворку: php relay/tests/run.php
declare(strict_types=1);
$GLOBALS['__fails'] = 0;
$GLOBALS['__total'] = 0;
function expect($actual, $expected, string $name): void
{
    $GLOBALS['__total']++;
    if ($actual === $expected) {
        return;
    }
    $GLOBALS['__fails']++;
    fwrite(STDERR, "FAIL $name: expected " . var_export($expected, true)
        . ", got " . var_export($actual, true) . "\n");
}

require __DIR__ . '/../mqtt_publish.php';

// --- кодування MQTT 3.1.1 ---
expect(mqtt_encode_string('ab'), "\x00\x02ab", 'encode_string length prefix');
expect(mqtt_encode_remaining_length(0), "\x00", 'remaining_length 0');
expect(mqtt_encode_remaining_length(127), "\x7f", 'remaining_length 127');
expect(mqtt_encode_remaining_length(128), "\x80\x01", 'remaining_length 128');
expect(mqtt_encode_remaining_length(321), "\xc1\x02", 'remaining_length 321');

$connect = mqtt_build_connect('cid', 'user', 'pw', 30);
expect(ord($connect[0]), 0x10, 'CONNECT packet type');
expect(substr($connect, 2, 8), "\x00\x04MQTT\x04\xc2", 'CONNECT protocol, level 4, flags user+pass+clean');
expect(substr($connect, 10, 2), "\x00\x1e", 'CONNECT keepalive 30');
expect(substr($connect, 12), "\x00\x03cid\x00\x04user\x00\x02pw", 'CONNECT payload');

$pub = mqtt_build_publish('t/x', 'hi', true, 1);
expect(ord($pub[0]), 0x33, 'PUBLISH type, QoS1, retain');
expect(ord($pub[1]), 2 + 3 + 2 + 2, 'PUBLISH remaining length');
expect(substr($pub, 2), "\x00\x03t/x\x00\x01hi", 'PUBLISH topic, packet id 1, payload');

$pubNoRetain = mqtt_build_publish('t', '', false, 7);
expect(ord($pubNoRetain[0]), 0x32, 'PUBLISH without retain');
expect(substr($pubNoRetain, -2), "\x00\x07", 'PUBLISH packet id 7');

require __DIR__ . '/map_tests.php';

$t = $GLOBALS['__total'];
$f = $GLOBALS['__fails'];
echo $f === 0 ? "OK $t/$t\n" : "FAILED $f of $t\n";
exit($f === 0 ? 0 : 1);
