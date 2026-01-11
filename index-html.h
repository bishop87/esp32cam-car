static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <meta charset="UTF-8">
    <!--<title>ESP32-CAM Robot</title>-->
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
      table { margin-left: auto; margin-right: auto; }
      td { padding: 8 px; }
      .button {
        background-color: #2f4468;
        border: none;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }
      img {  width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
    </style>
  </head>
  <body>
    <h1>ESP32-CAM Robot</h1>
    <img src="" id="photo" >
    <table><tbody>
      <tr>
        <td align="center" colspan="3"><label for="bpwm">Speed</label><input id="bpwm" type="range" min="800" max="1023" value="900" style="width:100%;"></td>
      </tr>
      <tr>
        <td align="center"><label for="angle" style="font-size: x-large;">⦡</label><input type="number" id="angle" name="angle" value="5" min="0" max="180"></td>
        <td align="center"><button class="button" onmousedown="toggleCheckbox('forward');" ontouchstart="toggleCheckbox('forward');" onmouseup="toggleCheckbox('stop');" ontouchend="toggleCheckbox('stop');">▲</button></td>
        <td align="center"><button class="button" onclick="toggleCheckbox('led');">☼</button></td></tr>
      <tr>
        <td align="center"><button class="button" onmousedown="toggleCheckbox('left');" ontouchstart="toggleCheckbox('left');" onmouseup="toggleCheckbox('stop');" ontouchend="toggleCheckbox('stop');">◄</button></td>
        <td align="center"><button class="button" onmousedown="toggleCheckbox('stop');" ontouchstart="toggleCheckbox('stop');">◼</button></td>
        <td align="center"><button class="button" onmousedown="toggleCheckbox('right');" ontouchstart="toggleCheckbox('right');" onmouseup="toggleCheckbox('stop');" ontouchend="toggleCheckbox('stop');">►</button></td>
      </tr>
      <tr>
        <td></td>
        <td align="center"><button class="button" onmousedown="toggleCheckbox('backward');" ontouchstart="toggleCheckbox('backward');" onmouseup="toggleCheckbox('stop');" ontouchend="toggleCheckbox('stop');">▼</button></td>
      </tr>
    </tbody></table>
   <script>
   function toggleCheckbox(x) {
     // Recupera i valori attuali dei campi di input numerici
     var bpwm_value = document.getElementById("bpwm").value;
     var angle_value = document.getElementById("angle").value;
     var url = "/action?go=" + x + "&bpwm=" + bpwm_value + "&angle=" + angle_value;

     var xhr = new XMLHttpRequest();
     xhr.open("GET", url, true);
     xhr.send();
   }
   window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
  </script>
  </body>
</html>
)rawliteral";
