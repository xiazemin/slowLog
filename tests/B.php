<?php
class B {
    function __construct () 
    { 
        var_dump('construct B');
    }
   public function func1(){
    usleep(60000);
      echo "function1";
    }
}
