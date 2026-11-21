<?php
/**
 * Логіка приймача вебхука без побічних ефектів HTTP: тестується напряму.
 * hook.php лише збирає $_GET/$_SERVER/тіло, викликає hook_handle і відповідає.
 */
declare(strict_types=1);

require_once __DIR__ . '/map.php';

const HOOK_LOG_MAX_LINE = 4096;

function hook_config_ok($cfg): bool
{
    return is_array($cfg)
        && is_string($cfg['secret'] ?? null)
        && is_string($cfg['topic'] ?? null) && $cfg['topic'] !== ''
        && is_array($cfg['mqtt'] ?? null);
}

function hook_key_ok(array $cfg, array $get, array $server): bool
{
    $secret = (string)($cfg['secret'] ?? '');
    if ($secret === '' || $secret === 'CHANGE_ME') {
        return false;
    }
    // Київ Цифровий шле auth_token як заголовок Authorization (сирим значенням).
    // CGI/FastCGI-хостинги не передають його в PHP напряму: .htaccess кладе
    // його в REDIRECT_HTTP_AUTHORIZATION через SetEnvIf.
    $auth = $server['HTTP_AUTHORIZATION']
        ?? ($server['REDIRECT_HTTP_AUTHORIZATION'] ?? ($server['PHP_AUTH_DIGEST'] ?? ''));
    if (is_string($auth) && stripos($auth, 'Bearer ') === 0) {
        $auth = substr($auth, 7);
    }
    $given = $get['key'] ?? ($server['HTTP_X_RELAY_KEY'] ?? $auth);
    return is_string($given) && hash_equals($secret, $given);
}

function hook_parse_payload(string $body, array $get, array $post): array
{
    $json = json_decode($body, true);
    if (is_array($json)) {
        return $json;
    }
    $form = $post ?: $get;
    unset($form['key']);
    return $form;
}

function hook_log_line(array $get, array $server, string $body, string $result): string
{
    $query = $get;
    unset($query['key']);
    $line = sprintf(
        "%s %s %s ct=%s qs=%s body=%s -> %s",
        date('c'),
        $server['REQUEST_METHOD'] ?? '-',
        $server['REMOTE_ADDR'] ?? '-',
        $server['CONTENT_TYPE'] ?? '-',
        http_build_query($query),
        str_replace(["\r", "\n"], ' ', $body),
        $result
    );
    if (strlen($line) > HOOK_LOG_MAX_LINE) {
        $suffix = ' ...[cut] -> ' . $result;
        $line = substr($line, 0, HOOK_LOG_MAX_LINE - strlen($suffix)) . $suffix;
    }
    return $line;
}

function hook_log_write(string $logDir, string $line): void
{
    if (!is_dir($logDir)) {
        @mkdir($logDir, 0750, true);
    }
    $ok = @file_put_contents($logDir . '/raw-' . date('Ym') . '.log', $line . "\n", FILE_APPEND | LOCK_EX);
    if ($ok === false) {
        error_log("hook: cannot write log in $logDir: $line");
    }
}

/**
 * @param callable $publish function(array $mqttCfg, string $topic, string $payload, bool $retain): bool
 * @return array [int $httpCode, array $jsonBody]
 */
function hook_handle($cfg, array $get, array $server, string $body, array $post, callable $publish, string $logDir): array
{
    if (!hook_config_ok($cfg)) {
        return [500, ['ok' => false, 'reason' => 'bad config']];
    }
    if (!hook_key_ok($cfg, $get, $server)) {
        return [403, ['ok' => false]];
    }

    $payload = hook_parse_payload($body, $get, $post);
    $state = map_payload_to_state($payload);
    if ($state === null) {
        hook_log_write($logDir, hook_log_line($get, $server, $body, 'unmapped'));
        return [202, ['ok' => false, 'reason' => 'unmapped']];
    }

    $msg = json_encode([
        'state' => $state,
        'ts' => time(),
        'src' => 'kyiv_webhook',
        'causes' => map_payload_causes($payload),
    ]);
    if (!$publish($cfg['mqtt'], (string)$cfg['topic'], $msg, true)) {
        hook_log_write($logDir, hook_log_line($get, $server, $body, 'publish failed'));
        error_log('hook: publish failed');
        return [502, ['ok' => false, 'reason' => 'publish failed']];
    }
    hook_log_write($logDir, hook_log_line($get, $server, $body, "$state published"));
    return [200, ['ok' => true, 'state' => $state]];
}
