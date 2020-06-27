--TEST--
Check for spl slowLog presence
--SKIPIF--
<?php if (!extension_loaded("slowLog")) print "skip"; ?>
--FILE--
<?php
spl_autoload_extensions('.php');
spl_autoload_register();
//echo "test\n";
new A();
$b=new B();
$b->func1();
?>
--EXPECT--
string(11) "construct A"
string(11) "construct B"
function1