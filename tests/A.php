<?php
class A {
    function __construct () 
    { 
        var_dump('construct A');
    }
   public function func2($i){
     if($i<=0){
     return;
     }
     usleep($i*10000);
     echo $i;
     return $this->func2($i-1);
   }
}
