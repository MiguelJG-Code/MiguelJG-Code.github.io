<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(200); exit; }

$db_host = 'your-db-host';
$db_name = 'your-db-name';
$db_user = 'your-db-user';
$db_pass = 'your-db-password';

define('TOKEN_SECRET', 'pigmart_samar_2025');
function makeToken($u){ return hash('sha256', $u.TOKEN_SECRET.date('Y-m-d')); }
function isAdmin($d){ $u=$d['username']??''; $t=$d['token']??''; return $u!==''&&hash_equals(makeToken($u),$t); }

try {
    $pdo = new PDO("mysql:host=$db_host;dbname=$db_name;charset=utf8", $db_user, $db_pass);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
} catch(PDOException $e) { die(json_encode(['error'=>'DB connection failed'])); }

// Ensure image_url column is MEDIUMTEXT (big enough for base64)
try { $pdo->query("ALTER TABLE pigs MODIFY image_url MEDIUMTEXT"); } catch(PDOException $e){}
// Add image_url column if missing
try { $pdo->query("ALTER TABLE pigs ADD COLUMN image_url MEDIUMTEXT AFTER description"); } catch(PDOException $e){}

$action = $_GET['action'] ?? '';
$data = json_decode(file_get_contents('php://input'), true) ?? [];

if ($action === 'fix_admin') {
    $h = password_hash('admin123', PASSWORD_BCRYPT);
    $c = $pdo->query("SELECT COUNT(*) FROM admins")->fetchColumn();
    if($c>0){ $s=$pdo->prepare("UPDATE admins SET password_hash=? WHERE username='admin'"); $s->execute([$h]); }
    else { $s=$pdo->prepare("INSERT INTO admins (username,password_hash) VALUES ('admin',?)"); $s->execute([$h]); }
    echo json_encode(['done'=>true,'verify_test'=>password_verify('admin123',$h)?'PASS':'FAIL']);
    exit;
}
elseif ($action === 'get_pigs') {
    $stmt = $pdo->query("SELECT * FROM pigs WHERE is_active=1 ORDER BY id DESC");
    echo json_encode($stmt->fetchAll(PDO::FETCH_ASSOC));
}
elseif ($action === 'get_settings') {
    $stmt = $pdo->query("SELECT * FROM settings");
    $rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
    $r = [];
    foreach($rows as $row) $r[$row['setting_key']] = $row['setting_value'];
    echo json_encode($r);
}
elseif ($action === 'submit_request') {
    $stmt = $pdo->prepare("INSERT INTO requests (customer_name,phone,email,pig_requested,delivery_location,payment_method) VALUES (?,?,?,?,?,?)");
    $stmt->execute([$data['name']??'',$data['phone']??'',$data['email']??'',$data['pig']??'',$data['location']??'',$data['payment']??'']);
    echo json_encode(['success'=>true,'id'=>$pdo->lastInsertId()]);
}
elseif ($action === 'admin_login') {
    $stmt = $pdo->prepare("SELECT * FROM admins WHERE username=?");
    $stmt->execute([$data['username']??'']);
    $admin = $stmt->fetch(PDO::FETCH_ASSOC);
    if($admin && password_verify($data['password']??'',$admin['password_hash'])){
        echo json_encode(['success'=>true,'token'=>makeToken($admin['username']),'username'=>$admin['username']]);
    } else {
        echo json_encode(['success'=>false,'message'=>'Invalid username or password']);
    }
}
elseif ($action === 'change_credentials') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->prepare("SELECT * FROM admins WHERE username=?");
    $stmt->execute([$data['username']]);
    $admin=$stmt->fetch(PDO::FETCH_ASSOC);
    if(!$admin||!password_verify($data['cur_password']??'',$admin['password_hash'])){
        echo json_encode(['success'=>false,'message'=>'Current password is incorrect.']);exit;
    }
    $newUser=trim($data['new_username']??$admin['username']);
    $newPass=$data['new_password']??'';
    $newHash=$newPass?password_hash($newPass,PASSWORD_BCRYPT):$admin['password_hash'];
    $pdo->prepare("UPDATE admins SET username=?,password_hash=? WHERE id=?")->execute([$newUser,$newHash,$admin['id']]);
    echo json_encode(['success'=>true]);
}
elseif ($action === 'get_requests') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->query("SELECT * FROM requests ORDER BY submitted_at DESC");
    echo json_encode($stmt->fetchAll(PDO::FETCH_ASSOC));
}
elseif ($action === 'confirm_request') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $pdo->prepare("INSERT INTO confirmed_requests (customer_name,phone,email,pig_requested,delivery_location,payment_method) SELECT customer_name,phone,email,pig_requested,delivery_location,payment_method FROM requests WHERE id=?")->execute([$data['id']]);
    $pdo->prepare("DELETE FROM requests WHERE id=?")->execute([$data['id']]);
    echo json_encode(['success'=>true]);
}
elseif ($action === 'decline_request') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $pdo->prepare("DELETE FROM requests WHERE id=?")->execute([$data['id']]);
    echo json_encode(['success'=>true]);
}
elseif ($action === 'get_confirmed') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->query("SELECT * FROM confirmed_requests ORDER BY confirmed_at DESC");
    echo json_encode($stmt->fetchAll(PDO::FETCH_ASSOC));
}
elseif ($action === 'delete_confirmed') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $pdo->prepare("DELETE FROM confirmed_requests WHERE id=?")->execute([$data['id']]);
    echo json_encode(['success'=>true]);
}
elseif ($action === 'save_settings') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->prepare("REPLACE INTO settings (setting_key,setting_value) VALUES (?,?)");
    foreach($data as $k=>$v){ if(in_array($k,['token','username']))continue; $stmt->execute([$k,$v]); }
    echo json_encode(['success'=>true]);
}
elseif ($action === 'add_pig') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->prepare("INSERT INTO pigs (name,breed,weight,age,gender,size,price,badge,badge_cls,description,image_url) VALUES (?,?,?,?,?,?,?,?,?,?,?)");
    $stmt->execute([$data['name']??'',$data['breed']??'',$data['wt']??0,$data['age']??'',$data['gender']??'',$data['size']??'',$data['price']??0,'New','status-new',$data['desc']??'',$data['image_url']??'']);
    echo json_encode(['success'=>true,'id'=>$pdo->lastInsertId()]);
}
elseif ($action === 'delete_pig') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $pdo->prepare("UPDATE pigs SET is_active=0 WHERE id=?")->execute([$data['id']]);
    echo json_encode(['success'=>true]);
}
elseif ($action === 'edit_pig') {
    if(!isAdmin($data)){echo json_encode(['error'=>'Unauthorized']);exit;}
    $stmt=$pdo->prepare("UPDATE pigs SET name=?,breed=?,weight=?,age=?,gender=?,size=?,price=?,description=?,image_url=? WHERE id=?");
    $stmt->execute([$data['name'],$data['breed']??'',$data['wt']??0,$data['age']??'',$data['gender']??'',$data['size']??'',$data['price'],$data['desc']??'',$data['image_url']??'',$data['id']]);
    echo json_encode(['success'=>true]);
}
else { echo json_encode(['error'=>'Unknown action']); }
?>
