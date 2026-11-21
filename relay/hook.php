<?php
/**
 * Приймач вебхука Порталу даних Києва.
 * URL для реєстрації: https://<домен>/air_alert/hook.php?key=<secret>
 * Відповіді: 403 невірний секрет, 202 тіло не розпізнано, 502 брокер
 * недоступний, 200 опубліковано.
 */
declare(strict_types=1);

require __DIR__ . '/mqtt_publish.php';
require __DIR__ . '/map.php';

const HOOK_LOG_MAX_BODY = 4096;

function hook_respond(int $code, array $body): void
{
    http_response_code($code);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($body, JSON_UNESCAPED_UNICODE), "\n";
    exit;
}

function hook_key_ok(array $cfg): bool
{
    $secret = (string)($cfg['secret'] ?? '');
    if ($secret === '' || $secret === 'CHANGE_ME') {
        return false;
    }
    $given = $_GET['key'] ?? ($_SERVER['HTTP_X_RELAY_KEY'] ?? '');
    return is_string($given) && hash_equals($secret, $given);
}

function hook_log_raw(string $body): void
{
    $dir = __DIR__ . '/log';
    if (!is_dir($dir)) {
        @mkdir($dir, 0750, true);
    }
    $query = $_GET;
    unset($query['key']);
    $line = sprintf(
        "%s %s %s ct=%s qs=%s body=%s\n",
        date('c'),
        $_SERVER['REQUEST_METHOD'] ?? '-',
        $_SERVER['REMOTE_ADDR'] ?? '-',
        $_SERVER['CONTENT_TYPE'] ?? '-',
        http_build_query($query),
        str_replace(["\r", "\n"], ' ', substr($body, 0, HOOK_LOG_MAX_BODY))
    );
    @file_put_contents($dir . '/raw-' . date('Ym') . '.log', $line, FILE_APPEND | LOCK_EX);
}

function hook_parse_payload(string $body): array
{
    $json = json_decode($body, true);
    if (is_array($json)) {
        return $json;
    }
    $form = $_POST ?: $_GET;
    unset($form['key']);
    return $form;
}

$cfg = @include __DIR__ . '/config.php';
if (!is_array($cfg)) {
    hook_respond(500, ['ok' => false, 'reason' => 'no config']);
}
if (!hook_key_ok($cfg)) {
    hook_respond(403, ['ok' => false]);
}

$body = (string)file_get_contents('php://input');
hook_log_raw($body);

$state = map_payload_to_state(hook_parse_payload($body));
if ($state === null) {
    hook_respond(202, ['ok' => false, 'reason' => 'unmapped']);
}

$msg = json_encode(['state' => $state, 'ts' => time(), 'src' => 'kyiv_webhook']);
if (!mqtt_publish($cfg['mqtt'], (string)$cfg['topic'], $msg, true)) {
    error_log('hook: publish failed');
    hook_respond(502, ['ok' => false, 'reason' => 'publish failed']);
}
hook_respond(200, ['ok' => true, 'state' => $state]);
