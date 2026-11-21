<?php
/**
 * Перевірка хостингу: чи випускає TLS-сокети до брокера і чи вірні креденшали.
 * GET https://<домен>/air_alert/selftest.php?key=<secret>
 * Публікує CLEAR у <topic>/selftest, не в live-топік.
 */
declare(strict_types=1);
ini_set('display_errors', '0');

require __DIR__ . '/mqtt_publish.php';
require __DIR__ . '/hook_lib.php';

header('Content-Type: application/json; charset=utf-8');

$cfg = @include __DIR__ . '/config.php';
if (!hook_config_ok($cfg)) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'reason' => 'bad config']), "\n";
    exit;
}
$secret = (string)($cfg['secret'] ?? '');
$given = $_GET['key'] ?? ($_SERVER['HTTP_X_RELAY_KEY'] ?? '');
if ($secret === '' || $secret === 'CHANGE_ME' || !is_string($given) || !hash_equals($secret, $given)) {
    http_response_code(403);
    echo json_encode(['ok' => false]), "\n";
    exit;
}

$topic = rtrim((string)$cfg['topic'], '/') . '/selftest';
$msg = json_encode(['state' => 'CLEAR', 'ts' => time(), 'src' => 'selftest']);
$t0 = microtime(true);
$published = mqtt_publish($cfg['mqtt'], $topic, $msg, false);
$logWritable = is_dir(__DIR__ . '/log') && is_writable(__DIR__ . '/log');
$mapOk = map_payload_to_state(['status' => 'тривога']) === 'RED';
$ok = $published && $logWritable && $mapOk;

http_response_code($ok ? 200 : 502);
echo json_encode([
    'ok' => $ok,
    'published' => $published,
    'log_writable' => $logWritable,
    'map_ok' => $mapOk,
    'mbstring' => extension_loaded('mbstring'),
    'topic' => $topic,
    'ms' => (int)round((microtime(true) - $t0) * 1000),
    'php' => PHP_VERSION,
    'openssl' => extension_loaded('openssl'),
    'broker' => ($cfg['mqtt']['host'] ?? '') . ':' . ($cfg['mqtt']['port'] ?? ''),
]), "\n";
