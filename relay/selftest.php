<?php
/**
 * Перевірка хостингу: чи випускає TLS-сокети до брокера і чи вірні креденшали.
 * GET https://<домен>/air_alert/selftest.php?key=<secret>
 * Публікує CLEAR у <topic>/selftest, не в live-топік.
 */
declare(strict_types=1);

require __DIR__ . '/mqtt_publish.php';

header('Content-Type: application/json; charset=utf-8');

$cfg = @include __DIR__ . '/config.php';
if (!is_array($cfg)) {
    http_response_code(500);
    echo json_encode(['ok' => false, 'reason' => 'no config']), "\n";
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
$ok = mqtt_publish($cfg['mqtt'], $topic, $msg, true);

http_response_code($ok ? 200 : 502);
echo json_encode([
    'ok' => $ok,
    'topic' => $topic,
    'ms' => (int)round((microtime(true) - $t0) * 1000),
    'php' => PHP_VERSION,
    'openssl' => extension_loaded('openssl'),
    'broker' => ($cfg['mqtt']['host'] ?? '') . ':' . ($cfg['mqtt']['port'] ?? ''),
]), "\n";
