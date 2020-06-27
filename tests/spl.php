<?php
spl_autoload_extensions('.php');
spl_autoload_register();
//echo "test\n";
$a=new A();
$a->func2(10);
$b=new B();
$b->func1();

function a(){
usleep(55000);
b();
}

function b(){
usleep(55000);
c();
}

function c(){
usleep(55000);
}

a();
