<?php
// Скопіювати в config.php і заповнити. config.php у .gitignore.
return [
    // Секрет, який має бути у ?key=... або заголовку X-Relay-Key. Згенерувати:
    //   php -r 'echo bin2hex(random_bytes(16)), PHP_EOL;'
    'secret' => 'CHANGE_ME',

    // Топік стану, на який підписана плата.
    'topic' => 'air_alert/kyiv/state',

    'mqtt' => [
        'host'    => 'xxxxxxxx.ala.eu-central-1.emqxsl.com',
        'port'    => 8883,
        'tls'     => true,
        'user'    => 'relay',
        'pass'    => 'CHANGE_ME',
        'timeout' => 5,
    ],
];
