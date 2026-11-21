<?php
/**
 * Приймач вебхука Порталу даних Києва.
 * URL для реєстрації: https://<домен>/air_alert/hook.php?key=<secret>
 * Відповіді: 403 невірний секрет, 202 тіло не розпізнано, 502 брокер
 * недоступний, 500 зламаний config.php, 200 опубліковано.
 */
declare(strict_types=1);
ini_set('display_errors', '0');

require __DIR__ . '/mqtt_publish.php';
require __DIR__ . '/hook_lib.php';

$cfg = @include __DIR__ . '/config.php';
$body = (string)file_get_contents('php://input');

[$code, $resp] = hook_handle($cfg, $_GET, $_SERVER, $body, $_POST, 'mqtt_publish', __DIR__ . '/log');

http_response_code($code);
header('Content-Type: application/json; charset=utf-8');
echo json_encode($resp, JSON_UNESCAPED_UNICODE), "\n";
