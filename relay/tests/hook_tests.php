<?php
// Тести hook_handle без мережі: публікація підміняється замиканням.
declare(strict_types=1);
require_once __DIR__ . '/../hook_lib.php';

$logDir = sys_get_temp_dir() . '/air_alert_hook_test_' . getmypid();
@mkdir($logDir);
array_map('unlink', glob("$logDir/*.log") ?: []);

$cfg = ['secret' => 's3cret', 'topic' => 'air_alert/kyiv/state',
        'mqtt' => ['host' => 'h', 'port' => 8883, 'user' => 'u', 'pass' => 'p', 'timeout' => 5]];
$published = [];
$okPublisher = function (array $mqtt, string $topic, string $payload, bool $retain) use (&$published): bool {
    $published[] = [$topic, json_decode($payload, true), $retain];
    return true;
};
$failPublisher = fn(array $mqtt, string $topic, string $payload, bool $retain): bool => false;

function req(string $method, array $get, string $body, array $headers = []): array
{
    $server = ['REQUEST_METHOD' => $method, 'REMOTE_ADDR' => '203.0.113.5', 'CONTENT_TYPE' => 'application/json'];
    foreach ($headers as $k => $v) {
        $server['HTTP_' . strtoupper(str_replace('-', '_', $k))] = $v;
    }
    return [$get, $server, $body];
}

[$get, $server, $body] = req('GET', [], '');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[0], 403, 'no key → 403');
expect(glob("$logDir/*.log"), [], '403 does not create log');

[$get, $server, $body] = req('POST', ['key' => 'wrong'], '{"status":"start"}');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[0], 403, 'wrong key → 403');
expect($published, [], '403 publishes nothing');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"start"}');
[$code, $resp] = hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir);
expect($code, 200, 'RED → 200');
expect($resp, ['ok' => true, 'state' => 'RED'], 'RED response body');
expect(count($published), 1, 'RED publishes once');
expect($published[0][0], 'air_alert/kyiv/state', 'publishes to configured topic');
expect($published[0][1]['state'], 'RED', 'payload state RED');
expect($published[0][1]['src'], 'kyiv_webhook', 'payload src');
expect(is_int($published[0][1]['ts']) && $published[0][1]['ts'] > 1700000000, true, 'payload ts is unix time');
expect($published[0][2], true, 'retained');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"x":1}');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir), [202, ['ok' => false, 'reason' => 'unmapped']], 'unmapped → 202');
expect(count($published), 1, 'unmapped publishes nothing');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"start"}');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[0], 200, 'repeat identical → 200 again');
expect(count($published), 2, 'repeat publishes again');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"end"}');
expect(hook_handle($cfg, $get, $server, $body, [], $failPublisher, $logDir), [502, ['ok' => false, 'reason' => 'publish failed']], 'broker down → 502');

[$get, $server, $body] = req('POST', [], '{"active":true}', ['X-Relay-Key' => 's3cret']);
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[1]['state'] ?? null, 'RED', 'header key accepted');

[$get, $server, $body] = req('GET', ['key' => 's3cret', 'status' => 'end'], '');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[1]['state'] ?? null, 'CLEAR', 'GET query form → CLEAR');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], 'not json');
expect(hook_handle($cfg, $get, $server, $body, ['active' => 'false'], $okPublisher, $logDir)[1]['state'] ?? null, 'CLEAR', 'form POST fallback');

// Секрет у заголовку Authorization (так його шле Київ Цифровий), сирий і з Bearer.
[$get, $server, $body] = req('POST', [], '{"current":{"state":1,"causes":["mig"]}}', ['Authorization' => 's3cret']);
[$code, $resp] = hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir);
expect($code, 200, 'raw Authorization header accepted');
expect(end($published)[1]['causes'] ?? null, ['mig'], 'causes forwarded in payload');
[$get, $server, $body] = req('POST', [], '{"current":{"state":0,"causes":[]}}', ['Authorization' => 'Bearer s3cret']);
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[0], 200, 'Bearer Authorization header accepted');
expect(end($published)[1]['causes'] ?? null, [], 'empty causes on clear');
[$get, $server, $body] = req('POST', [], '{"current":{"state":1}}', ['Authorization' => 'Bearer wrong']);
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir)[0], 403, 'wrong Bearer → 403');

// Лог: один рядок на запит, результат дописано, секрет відсутній, ≤ 4096 байт.
$log = file_get_contents(glob("$logDir/*.log")[0]);
$lines = array_filter(explode("\n", $log));
expect(count($lines), 9, 'one log line per authorized request (403s excluded)');
expect(strpos($log, 's3cret'), false, 'secret never logged');
expect(preg_match('/-> RED published/', $log) === 1, true, 'log records published result');
expect(preg_match('/-> unmapped/', $log) === 1, true, 'log records unmapped');
expect(preg_match('/-> publish failed/', $log) === 1, true, 'log records publish failure');
expect(preg_match('/qs=status=end/', $log) === 1, true, 'log has query string');

[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"start","pad":"' . str_repeat('z', 6000) . '"}');
hook_handle($cfg, $get, $server, $body, [], $okPublisher, $logDir);
$lines = array_filter(explode("\n", file_get_contents(glob("$logDir/*.log")[0])));
expect(strlen(end($lines)) <= 4096, true, 'log line capped at 4096 bytes');

// Конфіг без mqtt/topic → 500 bad config, без TypeError.
$badCfg = ['secret' => 's3cret'];
[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"start"}');
expect(hook_handle($badCfg, $get, $server, $body, [], $okPublisher, $logDir), [500, ['ok' => false, 'reason' => 'bad config']], 'incomplete config → 500');
expect(hook_config_ok(['secret' => 'x', 'topic' => 't', 'mqtt' => ['host' => 'h']]), true, 'config ok');
expect(hook_config_ok(['secret' => 'x', 'topic' => '', 'mqtt' => ['host' => 'h']]), false, 'empty topic not ok');
expect(hook_config_ok('nope'), false, 'non-array config not ok');

// Лог недоступний для запису: обробка триває, відповідь та сама.
[$get, $server, $body] = req('POST', ['key' => 's3cret'], '{"status":"start"}');
expect(hook_handle($cfg, $get, $server, $body, [], $okPublisher, '/proc/no_such_dir')[0], 200, 'unwritable log dir does not break handling');

array_map('unlink', glob("$logDir/*.log") ?: []);
@rmdir($logDir);
