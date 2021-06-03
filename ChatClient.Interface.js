var ChatClient_InitHardCtx = cwrap('ChatClient_InitHardCtx', null, ['string', 'string']);
var ChatClient_Connect = cwrap('ChatClient_Connect', null, ['string', 'string', 'string']);
var ChatClient_SendMsg = cwrap('ChatClient_SendMsg', null, ['string', 'string']);
var ChatClientCtx = {hostname: "", client: {} };
//--------------------------------------------------------------------------------
function ChatClient_LoginBtn(client_id) {
   var nick = document.getElementById("ChatClient_"+client_id+"_nick").value.trim();
   var host = ChatClientCtx.hostname;
   var error_field = document.getElementById("ChatClient_"+client_id+"_login_error");

   if (nick.length === 0) {
      error_field.style.display = 'block';
      error_field.innerHTML = "nick should be non-empty!";
      return;
   }

   error_field.style.display = 'none';
   ChatClient_Connect(client_id, nick, host);
}
//--------------------------------------------------------------------------------
function ChatClient_SendBtn(client_id) {
   var msg = document.getElementById("ChatClient_"+client_id+"_input").value.trim();

   if (msg.length === 0) return;
   ChatClient_SendMsg(client_id, msg);
   document.getElementById("ChatClient_"+client_id+"_input").value = "";
}
//--------------------------------------------------------------------------------
function ChatClient_DeployApplet(obj) {
   var client_id = obj.id;

   if (ChatClientCtx.client[client_id] == null) {

      ChatClientCtx.client[client_id] = {};
      ChatClientCtx.client[client_id].self_id = client_id;
      ChatClientCtx.client[client_id].room = obj.getAttribute("data-room");
      ChatClientCtx.client[client_id].title = obj.getAttribute("data-title");

      var applet_template = "" +
      "<div style='background: #000080; padding: 8px; color: #d0a518; font-weight: bold; font-family: Arial, Helvetica, sans-serif; '>" +
         "<table style='color: #d0a518; font-weight: bold; width: 100%; '><tr>"+
            "<td width='100%' >"+obj.getAttribute("data-title")+"</td>"+
         "</tr></table>"+
      "</div>"+
      // chat msgs tab
      "<div id='ChatClient_"+client_id+"_msgs_tab' style='display: none; border-style: solid; border-width: 1px;' >"+
         "<table width='100%' >"+
            "<tr><td colspan='2'><div id='ChatClient_"+client_id+"_history' style='overflow-y: scroll; height: 240px; width: 100%; ' ></div></td></tr>"+
            "<tr><td><input type='text' id='ChatClient_"+client_id+"_input' onkeydown='if (event.key == \"Enter\") ChatClient_SendBtn(\""+client_id+"\"); ' ></td><td><button onclick='ChatClient_SendBtn(\""+client_id+"\"); '>Send</button></td></tr>"+
         "</table>"+
      "</div>"+
      // login tab
      "<div id='ChatClient_"+client_id+"_login_tab' style='display: block; border-style: solid; border-width: 1px;' '>"+
         "<table width='100%' >"+
            "<tr><td><div id='ChatClient_"+client_id+"_login_error' style='display: none; background: #FF0000; color: #F0F0F0; text-align: center; '></div></td></tr>"+
            "<tr><td>User nick:</td></tr>"+
            "<tr><td><input type='text' id='ChatClient_"+client_id+"_nick' onkeydown='if (event.key == \"Enter\") ChatClient_LoginBtn(\""+client_id+"\"); ' ></td></tr>"+
            "<tr><td><button onclick='ChatClient_LoginBtn(\""+client_id+"\"); '>Login</button></td></tr>"+
         "</table>"+
      "</div>";
      obj.innerHTML = applet_template;
      ChatClient_InitHardCtx(client_id, ChatClientCtx.client[client_id].room);
   } else {
      var error_template = "" +
      "<div style='background: #FF0000; padding: 8px; color: #d0a518; font-weight: bold; font-family: Arial, Helvetica, sans-serif; '>" +
         "<table style='color: #101010; font-weight: bold; width: 100%; '><tr>"+
            "<td width='100%' >Error: Can't have duplicate clients on the same page. Sorry :( <br><br> You could assign it a different id.</td>"+
         "</tr></table>"+
      "</div>";
      obj.innerHTML = error_template;
   }

}
//--------------------------------------------------------------------------------

